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

#include "common/SourceFile.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "parser/ast/AstFormatter.hpp"
#include "stdlib/Print.hpp"
#include "test_config.h"

TEST_CASE("解释器 - Hello World") {
    Cial::Common::Result r{};

    Cial::Common::SourceFile sourceFile{};
    sourceFile.load(r, "println(\"Hello World!\");");
    Cial::Syntax::AstBuilder astBuilder{};
    Cial::Syntax::Parser parser{ sourceFile, astBuilder };
    auto *globalNode = parser.parse(r);

    Cial::Inter::SymbolTable globalTable{};
    Cial::Inter::IRGenerator codeGen{ sourceFile, globalTable };
    auto chunk = codeGen.parseAst(r, globalNode);

    Cial::Runtime rt;
    Cial::Bytecode::VMState vm{ globalTable, rt };

    vm.global("println", Cial::StdLib::S_PrintlnFunction);

    vm.allocCallFrame(chunk.get());
    vm.run();
}

TEST_CASE("解释器 - 执行测试") {
    Cial::Common::Result r{};

    Cial::Common::SourceFile sourceFile{ TEST_FILES_PATH R"(/startup.tjs)" };
    sourceFile.load(r);

    Cial::Syntax::AstBuilder astBuilder{};
    Cial::Syntax::Parser parser{ sourceFile, astBuilder };
    auto *globalNode = parser.parse(r);

    Cial::Inter::SymbolTable globalTable{};
    Cial::Inter::IRGenerator codeGen{ sourceFile, globalTable };
    auto chunk = codeGen.parseAst(r, globalNode);

    Cial::Runtime rt;
    Cial::Bytecode::VMState vm{ globalTable, rt };

    vm.global("println", Cial::StdLib::S_PrintlnFunction);

    vm.allocCallFrame(chunk.get());
    vm.run();
}

TEST_CASE("解释器 - 脚本执行性能") {
    BENCHMARK("fib 15") {
        Cial::Common::Result r{};

        Cial::Common::SourceFile sourceFile{};
        sourceFile.load(r, R"(
            function fib(n) {
                if(n < 2) return n;
                return fib(n - 2) + fib(n - 1);
            }
            fib(15);
        )");
        Cial::Syntax::AstBuilder astBuilder{};
        Cial::Syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        Cial::Inter::SymbolTable globalTable{};
        Cial::Inter::IRGenerator codeGen{ sourceFile, globalTable };
        auto chunk = codeGen.parseAst(r, globalNode);

        Cial::Runtime rt;
        Cial::Bytecode::VMState vm{ globalTable, rt };

        vm.allocCallFrame(chunk.get());
        vm.run();
    };
}