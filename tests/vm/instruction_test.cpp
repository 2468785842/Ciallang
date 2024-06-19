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

#include "vm/Interpreter.hpp"

#include "init/GlogInit.hpp"
#include "parser/Parser.hpp"
#include "ast/AstFormatter.hpp"
#include "gen/BytecodeGen.hpp"
#include "common/SourceFile.hpp"

int main(int argc, char** argv) {
    // Initialize Google's logging library.
    Ciallang::Init::InitializeGlog(argv);
    // Initialize GoogleTest.
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

TEST(Interpreter, Execute) {
    Ciallang::Common::Result r{};

    Ciallang::Common::SourceFile sourceFile{ R"(.\startup.tjs)" };
    sourceFile.load(r);

    Ciallang::Syntax::AstBuilder astBuilder{};
    Ciallang::Syntax::Parser parser{ sourceFile, astBuilder };
    auto* globalNode = parser.parse(r);
    Ciallang::AstFormatter formatter{};
    formatter.formatAst(globalNode);

    Ciallang::Inter::BytecodeGen codeGen{ sourceFile };
    auto chunk = codeGen.parseAst(r, globalNode);

    Ciallang::Bytecode::Interpreter interpreter{};

    fmt::println("{}", interpreter.dumpInstruction(*chunk));
    interpreter.run(chunk.get());
    fmt::println("{}", interpreter.dumpRegisters());
}
