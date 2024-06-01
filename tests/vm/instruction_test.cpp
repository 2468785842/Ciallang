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

#include <glog/logging.h>
#include <init/GlogInit.hpp>
using namespace Ciallang::VM;

int main(int argc, char** argv) {
    // Initialize Google's logging library.
    Ciallang::Init::InitializeGlog(argv);
    // Initialize GoogleTest.
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

TEST(VM_Test, Instruction) {
    VMChunk vmChunk{ "test" };

    auto index = vmChunk.load(Ciallang::tjsReal(1.2));

    vmChunk.emit(0, Opcodes::Const, { static_cast<uint8_t>(index) });
    vmChunk.emit(1, Opcodes::Ret);

    vmChunk.disassemble();
}
