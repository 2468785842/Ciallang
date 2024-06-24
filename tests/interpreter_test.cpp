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

#include "../src/vm/Interpreter.hpp"

#include "../src/parser/Parser.hpp"
#include "../src/ast/AstFormatter.hpp"
#include "../src/gen/BytecodeGen.hpp"
#include "../src/common/SourceFile.hpp"

TEST(InterpreterTest, TestExecute) {
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
