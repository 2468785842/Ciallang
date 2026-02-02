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
    class [[nodiscard]] Register {
    public:
        explicit Register() = default;
        explicit Register(const i64 index) : _index(index) {}

        [[nodiscard]] i64 index() const noexcept { return _index; }

        auto operator<=>(const Register &rhs) const = default;

    private:
        i64 _index;

        friend std::ostream &operator<<(std::ostream &os, const Register &reg) { return os << '%' << reg._index; }
    };
} // namespace cial::inter

namespace cial {
    using OptReg = Opt<inter::Register>;
}

template <>
struct fmt::formatter<cial::inter::Register> : ostream_formatter {};

template <>
struct std::hash<cial::inter::Register> {
    size_t operator()(const cial::inter::Register r) const noexcept { return std::hash<size_t>{}(r.index()); }
};