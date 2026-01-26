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

namespace cial::Inter {
    class [[nodiscard]] Register {
    public:
        explicit Register() = default;
        explicit Register(const u16 index) : _index(index) {}

        [[nodiscard]] u16 index() const noexcept { return _index; }
        bool operator==(const Register &) const noexcept = default;

    private:
        u16 _index;

        friend std::ostream &operator<<(std::ostream &os, const Register &reg) { return os << '%' << reg._index; }
    };
} // namespace cial::Inter

namespace cial {
    using OptReg = Opt<Inter::Register>;
}

template <>
struct fmt::formatter<cial::Inter::Register> : ostream_formatter {};
