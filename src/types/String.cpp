//
// Created by LiDong on 2025/10/6.
//

#include "String.hpp"

namespace Ciallang {

    String::String(const char *str, std::uint32_t len) : _len(len) {
        char *buf = _shortStr;
        if (len > G_ShortStrLen) {
            _longStr = new char[len + 1];
            buf = _longStr;
        }
        buf[len] = '\0';
        std::memcpy(buf, str, len);
    }

} // namespace Ciallang