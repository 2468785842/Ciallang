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

#include <string>
#include <vector>

#include "SourceLocation.hpp"

namespace cial::Common {

    class ResultMessage {
    public:
        enum Types { info, error, warning, data };

        explicit ResultMessage(std::string message, const SourceLocation loc = {}, std::string details = "",
                               const Types type = info) :
            _type(type), _message(std::move(message)), _details(std::move(details)), _location(loc) {}

        [[nodiscard]] Types type() const noexcept { return _type; }

        [[nodiscard]] bool isError() const noexcept { return _type == error; }

        [[nodiscard]] const std::string &details() const noexcept { return _details; }

        [[nodiscard]] const std::string &message() const noexcept { return _message; }

        [[nodiscard]] const SourceLocation &location() const noexcept { return _location; }

    private:
        Types _type;
        std::string _message{};
        std::string _details{};
        SourceLocation _location{};
    };

    using ResultMessageList = std::vector<ResultMessage>;
} // namespace cial::Common
