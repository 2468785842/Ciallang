//
// Created by LiDong on 2025/10/6.
//

#include "String.hpp"

namespace Cial {

    String::String(const char *str, const std::uint32_t len) : _len(len) {
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

} // namespace Cial