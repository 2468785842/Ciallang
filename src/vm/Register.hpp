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

#include "pch.h"

namespace Ciallang::Bytecode {
    class [[nodiscard]] Register {
    public:
        explicit Register(const std::uint32_t index) : _index(index) {
        }

        std::uint32_t index() const noexcept { return _index; }

    private:
        std::uint32_t _index;

        friend std::ostream& operator<<(std::ostream& os, const Register& reg) {
            return os << '%' << reg._index;
        }
    };
}

template <>
struct fmt::formatter<Ciallang::Bytecode::Register> : ostream_formatter {
};
