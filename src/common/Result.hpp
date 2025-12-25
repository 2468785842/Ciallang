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

#include "ResultMessage.hpp"
#include "SourceLocation.hpp"

#include "logging/Logger.hpp"

namespace Cial::Common {
    using namespace std;

    class [[nodiscard]] Result {
    public:
        Result() = default;

        constexpr void fail() noexcept { _success = false; }

        constexpr void succeed() noexcept { _success = true; }

        void info(const string &message, const SourceLocation &loc = {}, const string &details = {}) {
            CLL_LOG_INFO(message.c_str());
            _messages.emplace_back(message, loc, details, ResultMessage::Types::info);
        }

        void error(const string &message, const SourceLocation &loc = {}, const string &details = {}) {
            CLL_LOG_ERROR(message.c_str());
            _messages.emplace_back(message, loc, details, ResultMessage::Types::error);
            fail();
        }

        void warning(const std::string &message, const SourceLocation &loc = {}, const std::string &details = {}) {
            CLL_LOG_WARN(message.c_str());
            _messages.emplace_back(message, loc, details, ResultMessage::Types::warning);
        }

        [[nodiscard]] constexpr bool isFailed() const noexcept { return !_success; }

        [[nodiscard]] constexpr const ResultMessageList &messages() const noexcept { return _messages; }

    private:
        bool _success = true;
        ResultMessageList _messages{};
    };
} // namespace Cial::Common
