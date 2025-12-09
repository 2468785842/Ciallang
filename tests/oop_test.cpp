//
// Created by LiDong on 2025/12/6.
//

#include <catch.hpp>

#include "ast/AstBuilder.hpp"
#include "ast/AstFormatter.hpp"
#include "common/SourceFile.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "types/Object.hpp"
#include "vm/VMState.hpp"

using namespace Ciallang;

TEST_CASE("OOP - 类声明与实例化") {
    Common::Result r{};
    Common::SourceFile sourceFile{};
    Syntax::AstBuilder astBuilder{};
    Syntax::Parser parser{ sourceFile, astBuilder };

    sourceFile.load(r, R"(
        class A { }
        var o = new A();
    )");

    auto *globalNode = parser.parse(r);
    REQUIRE(globalNode != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Inter::SymbolTable globalTable{};
    Inter::IRGenerator codeGen{ sourceFile, globalTable };
    auto chunk = codeGen.parseAst(r, globalNode);
    REQUIRE(chunk != nullptr);

    Bytecode::VMState vm{ globalTable };
    vm.allocCallFrame(chunk.get());
    vm.run();

    const auto idx = globalTable.getSymbolIndex("o");
    REQUIRE(idx.has_value());

    const auto &val = vm.global(*idx);
    REQUIRE(val.isObject());
    auto *inst = dynamic_cast<InstanceObject *>(val.toObject());
    REQUIRE(inst != nullptr);
    REQUIRE(std::string{ inst->klass()->name() } == "A");
}

TEST_CASE("OOP - 类方法定义与调用") {
    Common::Result r{};
    Common::SourceFile sourceFile{};
    Syntax::AstBuilder astBuilder{};
    Syntax::Parser parser{ sourceFile, astBuilder };

    SECTION("实例方法调用") {
        sourceFile.load(r, R"(
            class A { function add(x, y) { return x + y; } }
            var o = new A();
            var res = o.add(1, 2);
        )");
        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
        Inter::SymbolTable globalTable{};
        Inter::IRGenerator codeGen{ sourceFile, globalTable };
        auto chunk = codeGen.parseAst(r, globalNode);
        REQUIRE(chunk != nullptr);
        Bytecode::VMState vm{ globalTable };
        vm.allocCallFrame(chunk.get());
        vm.run();
        const auto idx = globalTable.getSymbolIndex("res");
        REQUIRE(idx);
        REQUIRE(vm.global(*idx).toInteger() == 3);
    }
}