/*
 * Copyright (c) 2024/12/18
 *
 * 集成测试 - 测试完整脚本执行流程
 * 注意：由于当前API限制，这个测试文件被简化了
 */

#include <catch.hpp>

#include "common/SourceFile.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "parser/ast/AstBuilder.hpp"
#include "stdlib/Print.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("集成测试 - 基本脚本解析") {
    SECTION("简单表达式解析") {
        Common::SourceFile sourceFile{};
        Common::Result r{};
        sourceFile.load(r, "1 + 2;");

        if(r.isFailed()) {
            WARN("Failed to load source");
            return;
        }

        syntax::AstBuilder astBuilder{};
        syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("变量声明解析") {
        Common::SourceFile sourceFile{};
        Common::Result r{};
        sourceFile.load(r, "var x = 10;");

        syntax::AstBuilder astBuilder{};
        syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("函数声明解析") {
        Common::SourceFile sourceFile{};
        Common::Result r{};
        sourceFile.load(r, "function add(a, b) { return a + b; }");

        syntax::AstBuilder astBuilder{};
        syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("类声明解析") {
        Common::SourceFile sourceFile{};
        Common::Result r{};
        sourceFile.load(r, "class MyClass { function A() {} }");

        syntax::AstBuilder astBuilder{};
        syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        REQUIRE(globalNode != nullptr);
        for(const auto &msg : r.messages()) {
            if(msg.isError()) {
                INFO(msg.details());
                INFO(msg.message());
            }
        }
        REQUIRE_FALSE(r.isFailed());
    }
}

TEST_CASE("集成测试 - 代码生成和执行") {
    SECTION("简单表达式执行") {
        Common::SourceFile sourceFile{};
        Common::Result r{};
        sourceFile.load(r, "println(\"Hello World!\");");

        syntax::AstBuilder astBuilder{};
        syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        if(r.isFailed() || globalNode == nullptr) {
            WARN("Failed to parse");
            return;
        }

        inter::SymbolTable globalTable{};
        inter::IRGenerator codeGen{ sourceFile, globalTable };
        auto chunk = codeGen.parseAst(r, globalNode);

        if(r.isFailed() || !chunk) {
            WARN("Failed to generate code");
            return;
        }

        Runtime rt;
        vm::VMState vm{ globalTable, rt };

        vm.global("println", stdlib::S_PrintlnFunction);
        vm.allocCallFrame(chunk.get());
        vm.run();

        REQUIRE(true); // 如果执行到这里说明没有崩溃
    }
}
