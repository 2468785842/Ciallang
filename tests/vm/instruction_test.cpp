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
#include <gen/CodeGen.hpp>
#include <gtest/gtest.h>

#include <types/TjsValue.hpp>

#include <vm/VMChunk.hpp>
#include <vm/Instruction.hpp>

#include <init/GlogInit.hpp>
#include <parser/Parser.hpp>

#include "common/SourceFile.hpp"

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
    Interpreter interpreter{ &vmChunk };

    uint8_t constant = vmChunk.load(Ciallang::tjsReal(1.2));
    vmChunk.emit(123, Opcodes::Const, { constant });

    constant = vmChunk.load(Ciallang::tjsReal(3.4));
    vmChunk.emit(123, Opcodes::Const, { constant });

    vmChunk.emit(123, Opcodes::Add);

    constant = vmChunk.load(Ciallang::tjsReal(5.6));
    vmChunk.emit(123, Opcodes::Const, { constant });

    vmChunk.emit(123, Opcodes::Div);
    // vmChunk.emit(123, Opcodes::LNot);
    vmChunk.emit(123, Opcodes::Ret);
    // vmChunk.disassemble();

    EXPECT_EQ(interpreter.run(), InterpretResult::OK);
}

TEST(Interpreter, Execute) {
    Ciallang::Common::Result r{};

    Ciallang::Common::SourceFile sourceFile{ R"(.\startup.tjs)" };
    sourceFile.load(r);
    CHECK(!r.isFailed());

    Ciallang::Syntax::AstBuilder astBuilder{};
    Ciallang::Syntax::Parser parser{ sourceFile, astBuilder };
    auto* globalNode = parser.parse(r);
    CHECK(globalNode != nullptr && !r.isFailed());

    VMChunk vmChunk{ sourceFile.path() };
    Ciallang::Inter::CodeGen codeGen{ &vmChunk };
    codeGen.loadAst(globalNode);

    Interpreter interpreter{ &vmChunk };
    interpreter.run();
}
