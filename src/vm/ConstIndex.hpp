/*
 * Copyright (c) 2025/12/18 下午6:45
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

namespace Ciallang::Bytecode {
    class [[nodiscard]] ConstIndex {
    public:
        explicit ConstIndex() : _index(0) {}
        explicit ConstIndex(const size_t index) : _index(index) {}

        [[nodiscard]] size_t index() const noexcept { return _index; }

    private:
        size_t _index;

        friend std::ostream &operator<<(std::ostream &os, const ConstIndex &reg) { return os << '*' << reg._index; }
    };
} // namespace Ciallang::Bytecode

template <>
struct fmt::formatter<Ciallang::Bytecode::ConstIndex> : ostream_formatter {};
