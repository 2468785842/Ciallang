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

#include <sstream>

#include <fmt/format.h>

#include "Colorizer.hpp"

namespace Ciallang::Common {

    bool G_ColorEnabled = true;

    std::string Colorizer::colorize(
            const std::string &text,
            const fmt::color fgColor) {
        if (!G_ColorEnabled)
            return text;
        return fmt::format(
                fg(fgColor),
                "{}",
                text);
    }

    std::string Colorizer::colorize(
            const std::string &text,
            const fmt::color fgColor,
            const fmt::color bgColor) {
        if (!G_ColorEnabled)
            return text;
        return fmt::format(
                fg(fgColor) | bg(bgColor),
                "{}",
                text);
    }

    std::string Colorizer::colorizeRange(
            const std::string &text,
            const size_t begin,
            const size_t end,
            const fmt::color fgColor) {
        if (!G_ColorEnabled)
            return text;

        std::stringstream coloredSource;

        coloredSource << text.substr(0, begin)
                      << colorize(text.substr(begin, end - begin), fgColor)
                      << text.substr(end, text.length() - end);

        return coloredSource.str();

    }

    std::string Colorizer::colorizeRange(
            const std::string &text,
            const size_t begin,
            const size_t end,
            const fmt::color fgColor,
            const fmt::color bgColor) {
        if (!G_ColorEnabled)
            return text;

        std::stringstream coloredSource;

        coloredSource << text.substr(0, begin)
                      << colorize(text.substr(begin, end - begin), fgColor, bgColor)
                      << text.substr(end, text.length() - end);

        return coloredSource.str();
    }

}