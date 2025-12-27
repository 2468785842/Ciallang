//
// Created by LiDong on 2025/12/6.
//

#include <catch.hpp>

#include "ast/AstFormatter.hpp"
#include "common/SourceFile.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "types/Class.hpp"
#include "types/Object.hpp"
#include "vm/VMState.hpp"

using namespace Cial;

TEST_CASE("OOP - 类声明与实例化") {
    Common::Result r{};
    Common::SourceFile sourceFile{};
    Runtime rt;

    Syntax::Parser parser{ rt, sourceFile };

    sourceFile.load(r, R"(
        class A { }
        var o = new A();
    )");

    auto *globalNode = parser.parse(r);
    REQUIRE(globalNode != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Inter::IRGenerator codeGen{ sourceFile };
    auto chunk = codeGen.parseAst(r, globalNode);
    REQUIRE(chunk != nullptr);

    Bytecode::VMState vm{ rt };
    vm.allocCallFrame(chunk.get());
    vm.run();

    const auto &val = vm.global("o");
    REQUIRE(val.isObject());
    auto *inst = dynamic_cast<InstanceObject *>(val.toObject());
    REQUIRE(inst != nullptr);
    REQUIRE(rt.atomTable.get(inst->klass()->getName())->str[0] == 'A');
}

TEST_CASE("OOP - 类方法定义与调用") {
    Common::Result r{};
    Common::SourceFile sourceFile{};
    Syntax::AstBuilder astBuilder{};
    AtomTable atomTable{};
    OctetTable octetTable{};
    Runtime rt;

    Syntax::Parser parser{ rt, sourceFile };

    SECTION("实例方法调用") {

        sourceFile.load(r, R"(
            class A { function add(x, y) { return x + y; } }
            var o = new A();
            var res = o.add(1, 2);
        )");
        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
        Inter::IRGenerator codeGen{ sourceFile };
        auto chunk = codeGen.parseAst(r, globalNode);
        REQUIRE(chunk != nullptr);

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("res").toInteger() == 3);
    }

    SECTION("实例成员访问") {

        sourceFile.load(r, R"(
            class A { var a = 1; }
            var o = new A();
            var res = o.a;
        )");
        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
        Inter::IRGenerator codeGen{ sourceFile };
        auto chunk = codeGen.parseAst(r, globalNode);
        REQUIRE(chunk != nullptr);

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("res").toInteger() == 1);
    }
}


TEST_CASE("OOP - 类访问变量") {
    Common::Result r{};
    Common::SourceFile sourceFile{};
    Syntax::AstBuilder astBuilder{};
    AtomTable atomTable{};
    OctetTable octetTable{};

    Runtime rt;

    Syntax::Parser parser{ rt, sourceFile };

    SECTION("实例成员初始化变量访问") {

        sourceFile.load(r, R"(
            var a = 1;
            class A {
                var b = a;
            }
            var o = new A().b;
        )");

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        auto chunk = codeGen.parseAst(r, globalNode);
        REQUIRE(chunk != nullptr);

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("o").toInteger() == 1);
    }

    SECTION("实例成员方法初始化变量访问") {

        sourceFile.load(r, R"(
            var a = 1;
            class A {
                function b() { return a; }
            }
            var o = new A().b();
        )");

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        auto chunk = codeGen.parseAst(r, globalNode);
        REQUIRE(chunk != nullptr);

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("o").toInteger() == 1);
    }

    SECTION("实例成员函数访问成员变量") {

        sourceFile.load(r, R"(
            class A {
                var a = 1;
                function b() { return a; }
            }
            var o = new A().b();
        )");

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        auto chunk = codeGen.parseAst(r, globalNode);
        REQUIRE(chunk != nullptr);

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("o").toInteger() == 1);
    }
}
