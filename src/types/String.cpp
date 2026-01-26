//
// Created by LiDong on 2025/10/6.
//

#include "String.hpp"

#include "Value.hpp"

namespace cial {

    String::String(const char *str, const u32 len) : _len(len) {
        char *buf = _shortStr;
        if(len > SHORT_STR_LEN) {
            _longStr = new char[len + 1];
            buf = _longStr;
        }
        buf[len] = '\0';
        std::memcpy(buf, str, len);
    }


    String::~String() noexcept {
        if(this->_len > SHORT_STR_LEN) {
            delete[] _longStr;
        }
    }

    String::String(String &&str) noexcept : _len(str._len) {
        if(this->_len > SHORT_STR_LEN) {
            _longStr = str._longStr;
            str._longStr = nullptr;
        } else {
            _shortStr[this->_len] = '\0';
            std::memcpy(this->_shortStr, str._shortStr, this->_len);
        }
    }

    String &String::operator=(String &&str) noexcept {
        if(this != &str) {
            this->~String();
            new(this) String(std::move(str));
        }
        return *this;
    }

    String formatString(const String &fmt, size_t paramCount, Value *params) {
        std::string result;
        const char *f = fmt.getData();
        size_t in = 0;

        while(*f) {
            if(*f != '%') {
                result += *f++;
                continue;
            }

            f++; // skip '%'
            if(!*f)
                throw std::runtime_error("Invalid format string: ends with '%'");

            // parse flags (only support '-', '+', '#', '0')
            char flag = 0;
            if(*f == '-' || *f == '+' || *f == '#' || *f == '0') {
                flag = *f++;
            }

            // parse width
            Integer width = 0;
            if(*f == '*') {
                if(in >= paramCount)
                    throw std::runtime_error("Insufficient format parameters");
                width = params[in++].asInteger().unwrap();
                f++;
            } else {
                while(*f >= '0' && *f <= '9') {
                    width = width * 10 + (*f - '0');
                    f++;
                }
            }

            // parse precision
            Integer prec = -1;
            if(*f == '.') {
                f++;
                if(*f == '*') {
                    if(in >= paramCount)
                        throw std::runtime_error("Insufficient format parameters");
                    prec = params[in++].asInteger().unwrap();
                    f++;
                } else {
                    prec = 0;
                    while(*f >= '0' && *f <= '9') {
                        prec = prec * 10 + (*f - '0');
                        f++;
                    }
                }
            }

            // type
            char type = *f++;
            char buffer[1024] = {};

            switch(type) {
                case '%':
                    result += '%';
                    break;

                case 'c': {
                    if(in >= paramCount)
                        throw std::runtime_error("Insufficient parameters for %c");
                    char ch = static_cast<char>(params[in++].asInteger().unwrap());
                    result += ch;
                    break;
                }

                case 's': {
                    if(in >= paramCount)
                        throw std::runtime_error("Insufficient parameters for %s");
                    if(String *str = params[in++].asString().unwrap()) {
                        const char *sdata = str->getData();
                        size_t sLen = str->length();

                        if(size_t len =
                               prec >= 0 && static_cast<size_t>(prec) < sLen ? static_cast<size_t>(prec) : sLen;
                           width > 0 && static_cast<int>(len) < width) {
                            Integer pad = width - static_cast<int>(len);
                            if(flag == '-') {
                                result.append(sdata, len);
                                result.append(pad, ' ');
                            } else {
                                result.append(pad, ' ');
                                result.append(sdata, len);
                            }
                        } else {
                            result.append(sdata, len);
                        }
                    }
                    break;
                }

                case 'd':
                case 'i':
                case 'u':
                case 'o':
                case 'x':
                case 'X': {
                    if(in >= paramCount)
                        throw std::runtime_error("Insufficient parameters for integer format");
                    long long val = params[in++].asInteger().unwrap();
                    char fmtBuf[16];
                    if(prec >= 0)
                        std::snprintf(fmtBuf, sizeof(fmtBuf), "%%.%lldd", prec);
                    else
                        std::snprintf(fmtBuf, sizeof(fmtBuf), "%%d");
                    std::snprintf(buffer, sizeof(buffer), fmtBuf, val);
                    result += buffer;
                    break;
                }

                case 'f':
                case 'e':
                case 'g':
                case 'F':
                case 'E':
                case 'G': {
                    if(in >= paramCount)
                        throw std::runtime_error("Insufficient parameters for float format");
                    double val = params[in++].asReal().unwrap().value();
                    char fmtBuf[16];
                    if(prec >= 0)
                        std::snprintf(fmtBuf, sizeof(fmtBuf), "%%.%lld%c", prec, type);
                    else
                        std::snprintf(fmtBuf, sizeof(fmtBuf), "%%%c", type);
                    std::snprintf(buffer, sizeof(buffer), fmtBuf, val);
                    result += buffer;
                    break;
                }

                default:
                    throw std::runtime_error("Unsupported format type");
            }
        }

        return String(result.c_str(), static_cast<u32>(result.size()));
    }

    String String::escapeBackSlash() const {
        std::string ret;
        ret.reserve(_len * 2); // 预分配，避免频繁扩容

        auto appendHex = [&](const u8 c) {
            char buf[5]; // \xHH\0
            std::snprintf(buf, sizeof(buf), "\\x%02x", c);
            ret += buf;
        };

        for(u32 i = 0; i < _len; ++i) {
            switch(char c = getData()[i]) {
                case '\a':
                    ret += "\\a";
                    break;
                case '\b':
                    ret += "\\b";
                    break;
                case '\f':
                    ret += "\\f";
                    break;
                case '\n':
                    ret += "\\n";
                    break;
                case '\r':
                    ret += "\\r";
                    break;
                case '\t':
                    ret += "\\t";
                    break;
                case '\v':
                    ret += "\\v";
                    break;
                case '\\':
                    ret += "\\\\";
                    break;
                case '\'':
                    ret += "\\'";
                    break;
                case '\"':
                    ret += "\\\"";
                    break;
                default:
                    if(c < 0x20 || c > 0x7E) {
                        appendHex(c); // 控制字符或不可打印字符
                    } else {
                        ret += c; // 普通字符直接添加
                    }
                    break;
            }
        }

        return String(ret.c_str(), static_cast<u32>(ret.size()));
    }

    void String::append(const String &str) {
        if(str.isEmpty())
            return;

        const u32 newLen = _len + str._len;

        if(newLen < SHORT_STR_LEN) {
            auto i = this->_len;
            for(const char c : str) {
                this->_shortStr[i++] = c;
            }
        } else {
            auto *buf = new char[this->_len + str._len + 1];
            std::memcpy(buf, this->getData(), this->_len);
            delete[] this->_longStr;
            this->_longStr = buf;
            std::memcpy(this->_longStr + this->_len, str.getData(), str._len);
        }

        this->getBuffer()[newLen + 1] = '\0';
        this->_len = newLen;
    }

} // namespace cial