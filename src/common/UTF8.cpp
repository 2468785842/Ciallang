/*
 * Copyright (c) 2024/5/6 下午8:16
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */

#include "UTF8.hpp"
#include "utf8proc.h"

namespace Ciallang::Common {

    bool isRuneDigit(const int32_t r) {
        if(r < 0x80) {
            return isdigit(r) != 0;
        }
        return utf8proc_category(r) == UTF8PROC_CATEGORY_ND;
    }

    bool isRuneLetter(const int32_t r) {
        if(r < 0x80) {
            if(r == '_') return true;
            return isalpha(r) != 0;
        }
        switch(utf8proc_category(r)) {
        case UTF8PROC_CATEGORY_LU:
        case UTF8PROC_CATEGORY_LL:
        case UTF8PROC_CATEGORY_LT:
        case UTF8PROC_CATEGORY_LM:
        case UTF8PROC_CATEGORY_LO:
            return true;
        default:
            break;
        }
        return false;
    }

    bool isRuneWhitespace(const int32_t r) {
        switch(r) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return true;
        default:
            return false;
        }
    }

    EncodedRuneType utf8Encode(const int32_t r) {
        EncodedRuneType e{};

        const auto i = static_cast<uint32_t>(r);
        constexpr uint8_t mask = 0x3f;
        if(i <= (1 << 7) - 1) {
            e.data[0] = static_cast<uint8_t>(r);
            e.width = 1;
            return e;
        }

        if(i <= (1 << 11) - 1) {
            e.data[0] = static_cast<uint8_t>(r >> 6) | 0xc0;
            e.data[1] = static_cast<uint8_t>(r & mask) | 0x80;
            e.width = 2;
            return e;
        }

        if(i > runeMax || (i >= 0xd800 && i <= 0xdfff)) {
            e.data[0] = static_cast<uint8_t>(r >> 12) | 0xe0;
            e.data[1] = static_cast<uint8_t>(r >> 6) & mask | 0x80;
            e.data[2] = static_cast<uint8_t>(r) & mask | 0x80;
            e.value = runeInvalid;
            e.width = 3;
            return e;
        }

        if(i <= (1 << 16) - 1) {
            e.data[0] = static_cast<uint8_t>(r >> 12) | 0xe0;
            e.data[1] = static_cast<uint8_t>(r >> 6) & mask | 0x80;
            e.data[2] = static_cast<uint8_t>(r) & mask | 0x80;
            e.width = 3;
            return e;
        }

        e.data[0] = static_cast<uint8_t>(r >> 18) | 0xf0;
        e.data[1] = static_cast<uint8_t>(r >> 12) & mask | 0x80;
        e.data[2] = static_cast<uint8_t>(r >> 6) & mask | 0x80;
        e.data[3] = static_cast<uint8_t>(r) & mask | 0x80;
        e.width = 4;

        return e;
    }

    int64_t utf8Strlen(const std::string& str) {
        int64_t len = 0;
        auto p = str.data();
        for(; *p; len++) {
            const auto c = static_cast<uint8_t>(*p);

            size_t cp_size;
            if(c < 0x80)
                cp_size = 1;
            else if((c & 0xe0) == 0xc0)
                cp_size = 2;
            else if((c & 0xf0) == 0xe0)
                cp_size = 3;
            else if((c & 0xf8) == 0xf0)
                cp_size = 4;
            else
                return -1;

            p += cp_size;
        }
        return len;
    }

    CodePointType utf8Decode(const char* str, const size_t length) {
        CodePointType cp{};
        if(length == 0)
            return cp;

        const auto s0 = static_cast<uint8_t>(str[0]);
        const uint8_t x = S_Utf8_First[s0];

        if(x >= 0xf0) {
            const int32_t mask = (static_cast<int32_t>(x) << 31) >> 31;
            cp.value = (static_cast<int32_t>(s0) & (~mask)) | (runeInvalid & mask);
            cp.width = 1;
            return cp;
        }

        if(s0 < 0x80) {
            cp.value = s0;
            cp.width = 1;
            return cp;
        }

        const auto sz = static_cast<uint8_t>(x & 7);
        const Utf8AcceptRangeType accept = S_Utf8AcceptRanges[x >> 4];
        if(length < sizeof(sz))
            return cp;

        const auto b1 = static_cast<uint8_t>(str[1]);
        if(b1 < accept.low || accept.high < b1)
            return cp;

        if(sz == 2) {
            cp.value = (static_cast<int32_t>(s0) & 0x1f) << 6
                | (static_cast<int32_t>(b1) & 0x3f);
            cp.width = 2;
            return cp;
        }

        const auto b2 = static_cast<uint8_t>(str[2]);
        if(!(b2 >= 0x80 && b2 <= 0xbf))
            return cp;

        if(sz == 3) {
            cp.value = (static_cast<int32_t>(s0) & 0x1f) << 12
                | (static_cast<int32_t>(b1) & 0x3f) << 6
                | (static_cast<int32_t>(b2) & 0x3f);
            cp.width = 3;
            return cp;
        }

        const auto b3 = static_cast<uint8_t>(str[3]);
        if(!(b3 >= 0x80 && b3 <= 0xbf))
            return cp;

        cp.value = (static_cast<int32_t>(s0) & 0x07) << 18
            | (static_cast<int32_t>(b1) & 0x3f) << 12
            | (static_cast<int32_t>(b2) & 0x3f) << 6
            | (static_cast<int32_t>(b3) & 0x3f);
        cp.width = 4;

        return cp;
    }
}
