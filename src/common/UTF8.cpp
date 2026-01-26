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
#include <utf8proc.h>

namespace cial::Common {

    bool isRuneDigit(const i32 r) {
        if(r < 0x80) {
            return isdigit(r) != 0;
        }
        return utf8proc_category(r) == UTF8PROC_CATEGORY_ND;
    }

    bool isRuneLetter(const i32 r) {
        if(r < 0x80) {
            if(r == '_')
                return true;
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

    bool isRuneWhitespace(const i32 r) {
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

    EncodedRuneType utf8Encode(i32 r) {
        EncodedRuneType e{};
        e.value = r;

        // 非法码点 -> 使用 replacement character
        if(r < 0 || r > runeMax || (r >= 0xD800 && r <= 0xDFFF)) {
            r = runeInvalid;
            e.value = runeInvalid;
        }

        if(r <= 0x7F) { // 1 字节
            e.data[0] = static_cast<u8>(r);
            e.width = 1;
        } else if(r <= 0x7FF) { // 2 字节
            e.data[0] = 0xC0 | ((r >> 6) & 0x1F);
            e.data[1] = 0x80 | (r & 0x3F);
            e.width = 2;
        } else if(r <= 0xFFFF) { // 3 字节
            e.data[0] = 0xE0 | ((r >> 12) & 0x0F);
            e.data[1] = 0x80 | ((r >> 6) & 0x3F);
            e.data[2] = 0x80 | (r & 0x3F);
            e.width = 3;
        } else { // 4 字节
            e.data[0] = 0xF0 | ((r >> 18) & 0x07);
            e.data[1] = 0x80 | ((r >> 12) & 0x3F);
            e.data[2] = 0x80 | ((r >> 6) & 0x3F);
            e.data[3] = 0x80 | (r & 0x3F);
            e.width = 4;
        }

        return e;
    }


    int64_t utf8Strlen(const std::string &str) {
        int64_t len = 0;
        for(auto p = str.data(); *p; len++) {
            const auto c = static_cast<u8>(*p);

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
    CodePointType utf8Decode(const char *str, size_t length) {
        CodePointType cp;
        if(length == 0)
            return cp;

        u8 b0 = static_cast<u8>(str[0]);

        if(b0 <= 0x7F) { // 1 字节 ASCII
            cp.value = b0;
            cp.width = 1;
        } else if((b0 & 0xE0) == 0xC0) { // 2 字节
            if(length < 2)
                return cp;
            u8 b1 = static_cast<u8>(str[1]);
            if((b1 & 0xC0) != 0x80)
                return cp;

            i32 r = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
            if(r < 0x80)
                r = runeInvalid; // 避免过长编码
            cp.value = r;
            cp.width = 2;
        } else if((b0 & 0xF0) == 0xE0) { // 3 字节
            if(length < 3)
                return cp;
            u8 b1 = static_cast<u8>(str[1]);
            u8 b2 = static_cast<u8>(str[2]);
            if((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80)
                return cp;

            i32 r = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            if(r < 0x800 || (r >= 0xD800 && r <= 0xDFFF))
                r = runeInvalid;
            cp.value = r;
            cp.width = 3;
        } else if((b0 & 0xF8) == 0xF0) { // 4 字节
            if(length < 4)
                return cp;
            u8 b1 = static_cast<u8>(str[1]);
            u8 b2 = static_cast<u8>(str[2]);
            u8 b3 = static_cast<u8>(str[3]);
            if((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80)
                return cp;

            i32 r = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
            if(r < 0x10000 || r > runeMax)
                r = runeInvalid;
            cp.value = r;
            cp.width = 4;
        }

        return cp;
    }

} // namespace cial::Common
