/* * Copyright (c) 2024/5/6 下午8:16 *
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

#include <string>

#include <fmt/color.h>

namespace Ciallang::Common {

    // N.B. this is not thread safe
    extern bool G_ColorEnabled;

    class Colorizer {
    public:

        static std::string colorize(
                const std::string &,
                fmt::color);

        static std::string colorize(
                const std::string &,
                fmt::color,
                fmt::color);

        static std::string colorizeRange(
                const std::string &,
                size_t,
                size_t,
                fmt::color);

        static std::string colorizeRange(
                const std::string &,
                size_t,
                size_t,
                fmt::color,
                fmt::color);

    };
}
