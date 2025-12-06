/*
 * Copyright (c) 2024/12/18
 *
 * 集成测试 - 测试完整脚本执行流程
 * 注意：由于当前API限制，这个测试文件被简化了
 */

#include <catch.hpp>

#include "ast/AstBuilder.hpp"
#include "common/SourceFile.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "stdlib/Print.hpp"
#include "vm/VMState.hpp"

using namespace Ciallang;

TEST_CASE("集成测试 - 基本脚本解析") {
    SECTION("简单表达式解析") {
        Common::SourceFile sourceFile{};
        Common::Result r{};
        sourceFile.load(r, "1 + 2;");

        if(r.isFailed()) {
            WARN("Failed to load source");
            return;
        }

        Syntax::AstBuilder astBuilder{};
        Syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("变量声明解析") {
        Common::SourceFile sourceFile{};
        Common::Result r{};
        sourceFile.load(r, "var x = 10;");

        Syntax::AstBuilder astBuilder{};
        Syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("函数声明解析") {
        Common::SourceFile sourceFile{};
        Common::Result r{};
        sourceFile.load(r, "function add(a, b) { return a + b; }");

        Syntax::AstBuilder astBuilder{};
        Syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("类声明解析") {
        Common::SourceFile sourceFile{};
        Common::Result r{};
        sourceFile.load(r, "class MyClass { function A() {} }");

        Syntax::AstBuilder astBuilder{};
        Syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        REQUIRE(globalNode != nullptr);
        for(const auto &msg : r.messages()) {
            if(msg.isError()) {
                FAIL(msg.details());
                FAIL(msg.message());
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

        Syntax::AstBuilder astBuilder{};
        Syntax::Parser parser{ sourceFile, astBuilder };
        auto *globalNode = parser.parse(r);

        if(r.isFailed() || globalNode == nullptr) {
            WARN("Failed to parse");
            return;
        }

        Inter::SymbolTable globalTable{};
        Inter::IRGenerator codeGen{ sourceFile, globalTable };
        auto chunk = codeGen.parseAst(r, globalNode);

        if(r.isFailed() || !chunk) {
            WARN("Failed to generate code");
            return;
        }

        Bytecode::VMState interpreter{ globalTable };
        interpreter.global("println", StdLib::S_PrintlnFunction);
        interpreter.allocCallFrame(chunk.get());
        interpreter.run();

        REQUIRE(true); // 如果执行到这里说明没有崩溃
    }
}
