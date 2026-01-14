/*
 * Copyright (c) 2026/1/14.
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

#include "types/String.hpp"

namespace cial {

    class PreProcessor {
    public:
        explicit PreProcessor();

        ~PreProcessor() noexcept;

        [[nodiscard]] int getVar(const String &n) const;
        void setVar(const String &n, int v) const;

        void onSet(const String &expr) const;
        void onIf(const String &expr);
        void onEndIf();

        [[nodiscard]] bool isEnabled() const;

    private:
        struct Impl;
        Impl *_impl{};
        std::vector<bool> _ifStack;
    };
} // namespace cial