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
    class [[nodiscard]] Label {
    public:
        explicit Label(const size_t address): _address(address) {
        }

        size_t address() const { return _address; }

    private:
        const size_t _address;

        friend std::ostream& operator<<(std::ostream& os, const Label& label) {
            return os << '@' << label._address;
        }
    };
}

template <>
struct fmt::formatter<Ciallang::Bytecode::Label> : ostream_formatter {
};
