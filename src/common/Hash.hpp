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

#pragma once

#include <cstdint>

namespace Cial {

    inline std::uint32_t fnv1a(const char *s, const size_t len) {
        std::uint32_t h = 2166136261u;
        for(size_t i = 0; i < len; ++i) {
            h ^= static_cast<std::uint8_t>(s[i]);
            h *= 16777619u;
        }
        return h;
    }
} // namespace Cial