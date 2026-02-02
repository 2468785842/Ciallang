/*
 * Copyright (c) 2024/6/14 下午4:49
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

#include <fmt/ostream.h>

#include "types/Types.hpp"

namespace cial::inter {
    class [[nodiscard]] Label {
    public:
        explicit Label() = default;
        explicit Label(const i64 address) : _address(address) {}

        [[nodiscard]] i64 address() const noexcept { return _address; }

        auto operator<=>(const Label &rhs) const = default;

    private:
        i64 _address;

        friend std::ostream &operator<<(std::ostream &os, const Label &label) { return os << '@' << label._address; }
    };
} // namespace cial::inter

template <>
struct fmt::formatter<cial::inter::Label> : ostream_formatter {};

template <>
struct std::hash<cial::inter::Label> {
    size_t operator()(const cial::inter::Label &l) const noexcept { return std::hash<size_t>{}(l.address()); }
};