/*
 * Copyright (c) 2024/5/7 下午8:44
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
#include "VMChunk.hpp"

#include <fmt/format.h>

namespace Ciallang::VM {
    void VMChunk::disassemble() const {
        fmt::println("-------------- {} --------------", _rlc.name());

        for(size_t offset = 0; offset < count;) {
            const auto* inst = Instruction::instance(_bytecodes[offset]);
            fmt::println("{}", inst->disassemble(_bytecodes, _valuArray.constants, &_rlc, offset));
        }
    }
}
