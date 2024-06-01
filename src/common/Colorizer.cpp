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

#include <glog/logging.h>
#include <fmt/format.h>

#include "Colorizer.hpp"

namespace Ciallang::Common {
    static bool TerminalSupportsColor();

    bool G_ColorEnabled = TerminalSupportsColor();

    std::string Colorizer::colorize(const std::string& text,
                                    const fmt::color fgColor) {
        if(!G_ColorEnabled) return text;
        return fmt::format(fg(fgColor), "{}", text);
    }

    std::string Colorizer::colorize(const std::string& text,
                                    const fmt::color fgColor,
                                    const fmt::color bgColor) {
        if(!G_ColorEnabled) return text;
        return fmt::format(fg(fgColor) | bg(bgColor), "{}", text);
    }

    std::string Colorizer::colorizeRange(const std::string& text,
                                         const size_t begin,
                                         const size_t end,
                                         const fmt::color fgColor) {
        if(!G_ColorEnabled) return text;

        std::stringstream coloredSource;

        coloredSource << text.substr(0, begin)
                << colorize(text.substr(begin, end - begin), fgColor)
                << text.substr(end, text.length() - end);

        return coloredSource.str();
    }

    std::string Colorizer::colorizeRange(const std::string& text,
                                         const size_t begin,
                                         const size_t end,
                                         const fmt::color fgColor,
                                         const fmt::color bgColor) {
        if(!G_ColorEnabled)
            return text;

        std::stringstream coloredSource;

        coloredSource << text.substr(0, begin)
                << colorize(text.substr(begin, end - begin), fgColor, bgColor)
                << text.substr(end, text.length() - end);

        return coloredSource.str();
    }


    // borrow from glog
    // Returns true iff terminal supports using colors in output.
    static bool TerminalSupportsColor() {
        bool term_supports_color = false;
#ifdef GLOG_OS_WINDOWS
        // on Windows TERM variable is usually not set, but the console does
        // support colors.
        term_supports_color = true;
#else
  // On non-Windows platforms, we rely on the TERM variable.
  const char* const term = getenv("TERM");
  if (term != nullptr && term[0] != '\0') {
    term_supports_color =
        !strcmp(term, "xterm") || !strcmp(term, "xterm-color") ||
        !strcmp(term, "xterm-256color") || !strcmp(term, "screen-256color") ||
        !strcmp(term, "konsole") || !strcmp(term, "konsole-16color") ||
        !strcmp(term, "konsole-256color") || !strcmp(term, "screen") ||
        !strcmp(term, "linux") || !strcmp(term, "cygwin");
  }
#endif
        return term_supports_color;
    }
}
