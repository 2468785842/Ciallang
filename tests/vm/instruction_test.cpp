/*
 * Copyright (c) 2024/5/30 上午9:14
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
#include <gtest/gtest.h>

#include <types/TjsValue.hpp>

#include <vm/VMChunk.hpp>

TEST(VM_Test, Instruction) {
    Ciallang::VM::VMChunk vmChunk{ "test" };

    auto index = vmChunk.valueArray()->load(Ciallang::tjsReal(1.2));

    vmChunk.emit(Ciallang::VM::Opcodes::Const, 0);
    vmChunk.emit(static_cast<uint8_t>(index), 0);
    vmChunk.emit(Ciallang::VM::Opcodes::Ret, 2);
    vmChunk.disassemble();
}
