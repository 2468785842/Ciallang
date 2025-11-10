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

#include "vm/VMState.hpp"

#include "ast/AstFormatter.hpp"
#include "common/SourceFile.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "standard/Print.hpp"
#include "test_config.h"

TEST_CASE("解释器 - Hello World") {
    Ciallang::Common::Result r{};

    Ciallang::Common::SourceFile sourceFile{};
    sourceFile.load(r, "println(\"Hello World!\");");
    Ciallang::Syntax::AstBuilder astBuilder{};
    Ciallang::Syntax::Parser parser{ sourceFile, astBuilder };
    auto *globalNode = parser.parse(r);

    Ciallang::Inter::SymbolTable globalTable{};
    Ciallang::Inter::IRGenerator codeGen{ sourceFile, globalTable };
    auto chunk = codeGen.parseAst(r, globalNode);

    Ciallang::Bytecode::VMState interpreter{ globalTable };

    interpreter.global(&Ciallang::Standard::S_PrintlnFunction);

    interpreter.allocCallFrame(chunk.get());
    interpreter.run();
}

TEST_CASE("解释器 - 执行测试") {
    Ciallang::Common::Result r{};

    Ciallang::Common::SourceFile sourceFile{ TEST_FILES_PATH R"(/startup.tjs)" };
    sourceFile.load(r);

    Ciallang::Syntax::AstBuilder astBuilder{};
    Ciallang::Syntax::Parser parser{ sourceFile, astBuilder };
    auto *globalNode = parser.parse(r);
    Ciallang::AstFormatter formatter{};
    formatter.formatAst(globalNode);

    Ciallang::Inter::SymbolTable globalTable{};
    Ciallang::Inter::IRGenerator codeGen{ sourceFile, globalTable };
    auto chunk = codeGen.parseAst(r, globalNode);

    Ciallang::Bytecode::VMState interpreter{ globalTable };
    interpreter.global(&Ciallang::Standard::S_PrintlnFunction);

    fmt::println("{}", interpreter.dumpInstruction(*chunk));
    interpreter.allocCallFrame(chunk.get());
    interpreter.run();
    fmt::println("{}", interpreter.dumpRegisters());
}

TEST_CASE("解释器 - 脚本执行性能") {
    BENCHMARK("fib 15") {
        Ciallang::Common::Result r{};

        Ciallang::Common::SourceFile sourceFile{};
        sourceFile.load(r, R"(
            function fib(n) {
                if(n < 2) return n;
                return fib(n - 2) + fib(n - 1);
            }
            fib(15);
        )");
        Ciallang::Syntax::AstBuilder astBuilder{};
        Ciallang::Syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        Ciallang::Inter::SymbolTable globalTable{};
        Ciallang::Inter::IRGenerator codeGen{ sourceFile, globalTable };
        auto chunk = codeGen.parseAst(r, globalNode);

        Ciallang::Bytecode::VMState interpreter{ globalTable };

        interpreter.allocCallFrame(chunk.get());
        interpreter.run();
    };
}