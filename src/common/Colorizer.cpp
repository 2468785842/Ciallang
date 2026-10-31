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

#define _CRT_SECURE_NO_WARNINGS
#include "Colorizer.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace cial::Common {
    static bool TerminalSupportsColor();

    bool G_ColorEnabled = TerminalSupportsColor();

    std::string Colorizer::colorize(const std::string &text, const fmt::color fgColor) {
        if(!G_ColorEnabled)
            return text;
        return fmt::format(fg(fgColor), "{}", text);
    }

    std::string Colorizer::colorize(const std::string &text, const fmt::color fgColor, const fmt::color bgColor) {
        if(!G_ColorEnabled)
            return text;
        return fmt::format(fg(fgColor) | bg(bgColor), "{}", text);
    }

    std::string Colorizer::colorizeRange(const std::string &text, const size_t begin, const size_t end,
                                         const fmt::color fgColor) {
        if(!G_ColorEnabled)
            return text;

        std::stringstream coloredSource;

        coloredSource << text.substr(0, begin) << colorize(text.substr(begin, end - begin), fgColor)
                      << text.substr(end, text.length() - end);

        return coloredSource.str();
    }

    std::string Colorizer::colorizeRange(const std::string &text, const size_t begin, const size_t end,
                                         const fmt::color fgColor, const fmt::color bgColor) {
        if(!G_ColorEnabled)
            return text;

        std::stringstream coloredSource;

        coloredSource << text.substr(0, begin) << colorize(text.substr(begin, end - begin), fgColor, bgColor)
                      << text.substr(end, text.length() - end);

        return coloredSource.str();
    }


    // borrow from glog
    // Returns true iff terminal supports using colors in output.
    // 诊断走 stderr：以 stderr 的 TTY 状态判定；NO_COLOR（https://no-color.org）
    // 显式禁用；宿主可用 G_ColorEnabled 覆盖（如 --no-color）
    static bool TerminalSupportsColor() {
        if(const char *const noColor = getenv("NO_COLOR"); noColor != nullptr && noColor[0] != '\0')
            return false;

        bool term_supports_color = false;
#ifdef _WIN32
        term_supports_color = _isatty(_fileno(stderr)) != 0;
#else
        // On non-Windows platforms, we rely on the TERM variable.
        if(isatty(fileno(stderr)) != 0) {
            if(const char *const term = getenv("TERM"); term != nullptr && term[0] != '\0') {
                term_supports_color = !strcmp(term, "xterm") || !strcmp(term, "xterm-color") ||
                    !strcmp(term, "xterm-256color") || !strcmp(term, "screen-256color") || !strcmp(term, "konsole") ||
                    !strcmp(term, "konsole-16color") || !strcmp(term, "konsole-256color") || !strcmp(term, "screen") ||
                    !strcmp(term, "linux") || !strcmp(term, "cygwin");
            }
        }
#endif
        return term_supports_color;
    }
} // namespace cial::Common
