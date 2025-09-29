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

#include <catch.hpp>

#include "vm/Interpreter.hpp"

#include "ast/AstFormatter.hpp"
#include "common/SourceFile.hpp"
#include "gen/BytecodeGen.hpp"
#include "parser/Parser.hpp"
#include "test_config.h"
#include "standard/print.hpp"

TEST_CASE("Interpreter Test Execute") {
    Ciallang::Common::Result r{};

    Ciallang::Common::SourceFile sourceFile{ TEST_FILES_PATH R"(/startup.tjs)" };
    sourceFile.load(r);

    Ciallang::Syntax::AstBuilder astBuilder{};
    Ciallang::Syntax::Parser parser{ sourceFile, astBuilder };
    auto *globalNode = parser.parse(r);
    Ciallang::AstFormatter formatter{};
    formatter.formatAst(globalNode);

    Ciallang::Inter::BytecodeGen codeGen{ sourceFile };
    auto chunk = codeGen.parseAst(r, globalNode);

    Ciallang::Bytecode::Interpreter interpreter{};

    fmt::println("{}", interpreter.dumpInstruction(*chunk));
    interpreter.run(chunk.get());
    fmt::println("{}", interpreter.dumpRegisters());
}

TEST_CASE("Script execution benchmark") {
    BENCHMARK("fib 15") {
        Ciallang::Common::Result r{};

        Ciallang::Common::SourceFile sourceFile{};
        sourceFile.load(r, R"(
            function fib(n) {
                if(n < 2) return n;
                return fib(n - 2) + fib(n - 1);
            }
            println(fib(15));
        )");
        Ciallang::Syntax::AstBuilder astBuilder{};
        Ciallang::Syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        Ciallang::Inter::BytecodeGen codeGen{ sourceFile };
        auto chunk = codeGen.parseAst(r, globalNode);

        Ciallang::Bytecode::Interpreter interpreter{};
        interpreter.global(&Ciallang::Standard::S_PrintlnFunction);

        interpreter.run(chunk.get());
    };
}