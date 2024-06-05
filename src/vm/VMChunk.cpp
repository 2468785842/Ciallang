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

namespace Ciallang::VM {
    // void VMChunk::disassemble() const {
    //     #define RLC_NAME_LEN (_rlc.name().length() < 35 ? 35 - _rlc.name().length() : 35)
    //     #define RLC_NAME (RLC_NAME_LEN == 35 ? "-" : _rlc.name())
    //
    //     fmt::println("{0:-^{1}}", ' ' + RLC_NAME + ' ', RLC_NAME_LEN);
    //
    //     for(size_t offset = 0; offset < count;) {
    //         const auto* inst = Instruction::instance(
    //             static_cast<Opcodes>(bytecodes(offset))
    //         );
    //         fmt::println("{}", inst->disassemble(dataPool, constants(), &_rlc, offset));
    //         offset += inst->length();
    //     }
    // }
}
