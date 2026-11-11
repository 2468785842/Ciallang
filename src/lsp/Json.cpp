// Copyright (c) 2026/11/11
//
#include "lsp/Json.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace cial::lsp {
    namespace {
        class Parser {
        public:
            Parser(std::string_view text, std::string *error) : _text(text), _error(error) {}

            Json run() {
                try {
                    skipWs();
                    Json value = parseValue();
                    skipWs();
                    if(_pos != _text.size())
                        fail("trailing characters after JSON value");
                    return value;
                } catch(const std::runtime_error &failure) {
                    if(_error != nullptr)
                        *_error = failure.what();
                    return Json{};
                }
            }

        private:
            [[noreturn]] void fail(const char *reason) {
                throw std::runtime_error(std::string{reason} + " at offset " + std::to_string(_pos));
            }

            void skipWs() {
                while(_pos < _text.size()) {
                    const char ch = _text[_pos];
                    if(ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
                        break;
                    ++_pos;
                }
            }

            char peek() {
                if(_pos >= _text.size())
                    fail("unexpected end of input");
                return _text[_pos];
            }

            void expect(char ch) {
                if(peek() != ch)
                    fail("unexpected character");
                ++_pos;
            }

            bool literal(std::string_view word) {
                if(_text.substr(_pos, word.size()) != word)
                    return false;
                _pos += word.size();
                return true;
            }

            Json parseValue() {
                switch(peek()) {
                    case '{': return parseObject();
                    case '[': return parseArray();
                    case '"': return Json{parseString()};
                    default: break;
                }
                if(literal("true"))
                    return Json{true};
                if(literal("false"))
                    return Json{false};
                if(literal("null"))
                    return Json{nullptr};
                return parseNumber();
            }

            Json parseObject() {
                expect('{');
                Json value = Json::object();
                skipWs();
                if(peek() == '}') {
                    ++_pos;
                    return value;
                }
                for(;;) {
                    skipWs();
                    std::string key = parseString();
                    skipWs();
                    expect(':');
                    skipWs();
                    value.set(std::move(key), parseValue());
                    skipWs();
                    if(peek() == ',') {
                        ++_pos;
                        continue;
                    }
                    expect('}');
                    return value;
                }
            }

            Json parseArray() {
                expect('[');
                Json value = Json::array();
                skipWs();
                if(peek() == ']') {
                    ++_pos;
                    return value;
                }
                for(;;) {
                    skipWs();
                    value.push(parseValue());
                    skipWs();
                    if(peek() == ',') {
                        ++_pos;
                        continue;
                    }
                    expect(']');
                    return value;
                }
            }

            std::string parseString() {
                expect('"');
                std::string out;
                for(;;) {
                    if(_pos >= _text.size())
                        fail("unterminated string");
                    const char ch = _text[_pos++];
                    if(ch == '"')
                        return out;
                    if(ch != '\\') {
                        out.push_back(ch);
                        continue;
                    }
                    switch(peek()) {
                        case '"': out.push_back('"'); break;
                        case '\\': out.push_back('\\'); break;
                        case '/': out.push_back('/'); break;
                        case 'b': out.push_back('\b'); break;
                        case 'f': out.push_back('\f'); break;
                        case 'n': out.push_back('\n'); break;
                        case 'r': out.push_back('\r'); break;
                        case 't': out.push_back('\t'); break;
                        case 'u': {
                            ++_pos;
                            const unsigned point = hex4();
                            if(point >= 0xD800 && point <= 0xDBFF && _text.substr(_pos, 2) == "\\u") {
                                const auto escape = _pos;
                                _pos += 2;
                                const unsigned low = hex4();
                                if(low >= 0xDC00 && low <= 0xDFFF)
                                    appendUtf8(out, 0x10000 + ((point - 0xD800) << 10) + (low - 0xDC00));
                                else {
                                    appendUtf8(out, point); // 孤立代理按 WTF-8 语义原样编出
                                    _pos = escape;
                                }
                            } else {
                                appendUtf8(out, point);
                            }
                            break;
                        }
                        default: fail("invalid escape");
                    }
                    ++_pos;
                }
            }

            unsigned hex4() {
                if(_pos + 4 > _text.size())
                    fail("truncated \\u escape");
                unsigned value = 0;
                for(int i = 0; i < 4; ++i) {
                    const char ch = _text[_pos + i];
                    value <<= 4;
                    if(ch >= '0' && ch <= '9')
                        value |= static_cast<unsigned>(ch - '0');
                    else if(ch >= 'a' && ch <= 'f')
                        value |= static_cast<unsigned>(ch - 'a' + 10);
                    else if(ch >= 'A' && ch <= 'F')
                        value |= static_cast<unsigned>(ch - 'A' + 10);
                    else
                        fail("invalid hex digit");
                }
                _pos += 4;
                return value;
            }

            static void appendUtf8(std::string &out, unsigned point) {
                if(point < 0x80) {
                    out.push_back(static_cast<char>(point));
                } else if(point < 0x800) {
                    out.push_back(static_cast<char>(0xC0 | (point >> 6)));
                    out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
                } else if(point < 0x10000) {
                    out.push_back(static_cast<char>(0xE0 | (point >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
                } else {
                    out.push_back(static_cast<char>(0xF0 | (point >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((point >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (point & 0x3F)));
                }
            }

            Json parseNumber() {
                const size_t start = _pos;
                if(peek() == '-')
                    ++_pos;
                while(_pos < _text.size() &&
                      ((_text[_pos] >= '0' && _text[_pos] <= '9') || _text[_pos] == '.' || _text[_pos] == 'e' ||
                       _text[_pos] == 'E' || _text[_pos] == '+' || _text[_pos] == '-'))
                    ++_pos;
                if(_pos == start)
                    fail("invalid number");
                const double value = std::strtod(std::string{_text.substr(start, _pos - start)}.c_str(), nullptr);
                if(std::isnan(value) || std::isinf(value))
                    fail("number out of range");
                return Json{value};
            }

            std::string_view _text;
            std::string *_error;
            size_t _pos{};
        };

        void dumpString(std::string &out, const std::string &value) {
            out.push_back('"');
            for(const char ch : value) {
                switch(ch) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default:
                        if(static_cast<unsigned char>(ch) < 0x20) {
                            char escape[7];
                            std::snprintf(escape, sizeof(escape), "\\u%04x", static_cast<unsigned char>(ch));
                            out += escape;
                        } else {
                            out.push_back(ch);
                        }
                }
            }
            out.push_back('"');
        }

        void dumpValue(std::string &out, const Json &value) {
            switch(value.type()) {
                case Json::Type::Null: out += "null"; break;
                case Json::Type::Bool: out += value.asBool() ? "true" : "false"; break;
                case Json::Type::Number: {
                    const double number = value.asNumber();
                    if(number == static_cast<long long>(number) && std::fabs(number) < 9.0e15)
                        out += std::to_string(static_cast<long long>(number));
                    else
                        out += std::to_string(number);
                    break;
                }
                case Json::Type::String: dumpString(out, value.asString()); break;
                case Json::Type::Array: {
                    out.push_back('[');
                    bool first = true;
                    for(const auto &item : value.asArray()) {
                        if(!first)
                            out.push_back(',');
                        first = false;
                        dumpValue(out, item);
                    }
                    out.push_back(']');
                    break;
                }
                case Json::Type::Object: {
                    out.push_back('{');
                    bool first = true;
                    for(const auto &[key, item] : value.asObject()) {
                        if(!first)
                            out.push_back(',');
                        first = false;
                        dumpString(out, key);
                        out.push_back(':');
                        dumpValue(out, item);
                    }
                    out.push_back('}');
                    break;
                }
            }
        }
    } // namespace

    Json Json::parse(std::string_view text, std::string *error) {
        return Parser{text, error}.run();
    }

    std::string Json::dump() const {
        std::string out;
        dumpValue(out, *this);
        return out;
    }
} // namespace cial::lsp
