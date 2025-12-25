//
// Created by LiDong on 2025/10/6.
//

#include "String.hpp"

namespace Cial {

    String::String(const char *str, const std::uint32_t len) : _len(len) {
        char *buf = _shortStr;
        if(len > G_ShortStrLen) {
            _longStr = new char[len + 1];
            buf = _longStr;
        }
        buf[len] = '\0';
        std::memcpy(buf, str, len);
    }

    Value String::create(const char *str, const std::uint32_t size) { return Value{ new String(str, size) }; }

} // namespace Cial