//
// Created by LiDong on 2025/10/6.
//

#include <cstring>

#include "String.hpp"

#include "Value.hpp"

namespace cial {

    String::String(const char *str, const u32 len) : _shortStr{} {
        if(len <= SHORT_STR_LEN) {
            std::memcpy(_shortStr, str, len);
            _shortStr[len] = '\0';
            flags() = static_cast<u8>(len);
        } else {
            _long.data = new char[len + 1];
            _long.len = len;
            std::memcpy(_long.data, str, len);
            _long.data[len] = '\0';
            flags() = 0x10;
        }
    }

    String::~String() noexcept {
        if(isLong())
            delete[] _long.data;
    }

    String::String(String &&str) noexcept : _shortStr{} {
        const u8 mode = str.flags() & 0x1f;
        if(str.isLong()) {
            _long.data = str._long.data;
            _long.len = str._long.len;
        } else {
            std::memcpy(_shortStr, str._shortStr, sizeof(_shortStr));
        }
        flags() = mode;
        str.flags() &= 0xe0;
        str._shortStr[0] = '\0';
    }

    String &String::operator=(String &&str) noexcept {
        if(this != &str) {
            const u8 externalTag = flags() & 0xe0;
            this->~String();
            new(this) String(std::move(str));
            flags() |= externalTag;
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
        ret.reserve(length() * 2); // 预分配，避免频繁扩容

        auto appendHex = [&](const u8 c) {
            char buf[5]; // \xHH\0
            std::snprintf(buf, sizeof(buf), "\\x%02x", c);
            ret += buf;
        };

        for(u32 i = 0; i < length(); ++i) {
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

    void String::append(const String &str) { *this = concat(*this, str); }

} // namespace cial