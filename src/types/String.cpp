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
                    const Value &v = params[in++];
                    // TJS2 语义：%c 接受字符码或单字符字符串（语料 "%c".sprintf('b') == "b"）
                    if(v.isString()) {
                        const String *sv = v.asString().unwrap();
                        result += sv->isEmpty() ? ' ' : sv->getData()[0];
                    } else {
                        result += static_cast<char>(v.asInteger().unwrap());
                    }
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
                    const long long val = params[in++].asInteger().unwrap();
                    // 组装 printf 格式：%[flag][width][.prec]type（%04X 等 C 风格宽度/填充）
                    std::string fmtSpec = "%";
                    if(flag)
                        fmtSpec += flag;
                    if(width > 0)
                        fmtSpec += std::to_string(width);
                    if(prec >= 0)
                        fmtSpec += "." + std::to_string(prec);
                    fmtSpec += type;
                    std::snprintf(buffer, sizeof(buffer), fmtSpec.c_str(), val);
                    std::string num(buffer);
                    // flag=='0' 时 width 已由 printf 处理；未处理的不足宽度补空格（右对齐）
                    if(width > 0 && static_cast<int>(num.size()) < width && flag != '0' && flag != '-') {
                        result.append(static_cast<size_t>(width) - num.size(), ' ');
                    }
                    result += num;
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

        // SSO 阈值与构造/析护/move 的长串判据(len > SHORT_STR_LEN)一致:
        // newLen == SHORT_STR_LEN(21 字符+NUL 恰满 _shortStr)走 SSO。
        // 旧判据 < 会在 21 时分配 _longStr,随后析构/move 按短串处理——泄漏缓冲
        if(newLen <= SHORT_STR_LEN) {
            auto i = this->_len;
            for(const char c : str) {
                this->_shortStr[i++] = c;
            }
        } else {
            auto *buf = new char[newLen + 1];
            std::memcpy(buf, this->getData(), this->_len);
            delete[] this->_longStr;
            this->_longStr = buf;
            _lenCap = newLen;
            std::memcpy(this->_longStr + this->_len, str.getData(), str._len);
        }

        // 终止符写在 newLen 处(旧代码写 newLen+1:越界一位且真正终止符位置留垃圾)
        this->getBuffer()[newLen] = '\0';
        this->_len = newLen;
    }

} // namespace cial