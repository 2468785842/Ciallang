/*
 * Copyright (c) 2024/12/18
 *
 * 函数声明单元测试
 */

#include <catch.hpp>

#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace Cial;

TEST_CASE("函数声明 - 简单函数定义") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    sourceFile.load(r, R"(
        function empty() { }
        function foo() { return 42; }
        function add(a, b) { return a + b; }
    )");
    Runtime rt{};
    Syntax::Parser parser{ rt, sourceFile };

    auto *globalNode = parser.parse(r);
    REQUIRE(globalNode != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Inter::IRGenerator codeGen{ sourceFile };
    OptReg retReg{};
    auto chunk = codeGen.parseAst(r, globalNode, retReg);
    REQUIRE(chunk != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Bytecode::VMState vmState{ rt };
    vmState.allocCallFrame(chunk.get());
    vmState.run();

    VM vm{ &vmState };

    REQUIRE(vm.getGlobal("empty"_str).value.toObject());
    REQUIRE(vm.getGlobal("foo"_str).value.toObject());
    REQUIRE(vm.getGlobal("add"_str).value.toObject());

    REQUIRE(vm.evalExpr("empty()"_str).value.isVoid());

    VM::Handle rFoo = vm.evalExpr("foo()"_str);
    REQUIRE(rFoo.value.isInteger());
    REQUIRE(rFoo.value.toInteger() == 42);

    VM::Handle rAdd = vm.evalExpr("add(1, 2)"_str);

    REQUIRE(rAdd.value.isInteger());
    REQUIRE(rAdd.value.toInteger() == 3);
}

TEST_CASE("函数声明 - 参数列表") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    sourceFile.load(r, R"(
        function single(x) { return x; } var rSingle = single(1);
        function multi(a, b, c) { return a + b + c; } var rMulti = multi(1, 2, 3);
        function complex(param1, param_2, param3) { return param1; } var rComplex1 = complex(); var rComplex2 = complex(1); var rComplex3 = complex(1, 2, 3);
    )");
    Runtime rt{};
    Syntax::Parser parser{ rt, sourceFile };

    auto *globalNode = parser.parse(r);
    REQUIRE(globalNode != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Inter::IRGenerator codeGen{ sourceFile };
    OptReg retReg{};
    auto chunk = codeGen.parseAst(r, globalNode, retReg);
    REQUIRE(chunk != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Bytecode::VMState vm{ rt };
    vm.allocCallFrame(chunk.get());
    vm.run();

    REQUIRE(vm.global("single").isObject());
    REQUIRE(vm.global("multi").isObject());
    REQUIRE(vm.global("complex").isObject());

    Value rSingle = vm.global("rSingle");
    REQUIRE(rSingle.isInteger());
    REQUIRE(rSingle.toInteger() == 1);

    Value rMulti = vm.global("rMulti");

    REQUIRE(rMulti.isInteger());
    REQUIRE(rMulti.toInteger() == 6);

    Value rComplex1 = vm.global("rComplex1");

    REQUIRE(rComplex1.isVoid());

    Value rComplex2 = vm.global("rComplex2");

    REQUIRE(rComplex2.isInteger());
    REQUIRE(rComplex2.toInteger() == 1);

    Value rComplex3 = vm.global("rComplex3");

    REQUIRE(rComplex3.isInteger());
    REQUIRE(rComplex3.toInteger() == 1);
}

TEST_CASE("函数声明 - 函数体内容") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("带变量声明的函数体") {
        sourceFile.load(r, "function test() { var x = 10; var y = 20; return x + y; } var rTest = test();");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        OptReg retReg{};
        auto chunk = codeGen.parseAst(r, globalNode, retReg);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("test").isObject());

        Value rTest = vm.global("rTest");
        REQUIRE(rTest.isInteger());
        REQUIRE(rTest.toInteger() == 30);
    }

    SECTION("带控制流的函数体") {
        sourceFile.load(r, "function control() { if (true) return 1; else return 0; } var rControl = control();");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        OptReg retReg{};
        auto chunk = codeGen.parseAst(r, globalNode, retReg);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("control").isObject());
        Value rControl = vm.global("rControl");
        REQUIRE(rControl.isInteger());
        REQUIRE(rControl.toInteger() == 1);
    }

    SECTION("带循环的函数体") {
        sourceFile.load(r, "function loop() { var i = 0; while (i < 10) i = i + 1; return i; } var rLoop = loop();");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        OptReg retReg{};
        auto chunk = codeGen.parseAst(r, globalNode, retReg);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("loop").isObject());
        Value rLoop = vm.global("rLoop");
        REQUIRE(rLoop.isInteger());
        REQUIRE(rLoop.toInteger() == 10);
    }
}

TEST_CASE("函数声明 - 函数调用") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("无参数函数调用") {
        sourceFile.load(r, "function foo() { return 1; } var rFoo = foo();");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        OptReg retReg{};
        auto chunk = codeGen.parseAst(r, globalNode, retReg);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("foo").isObject());

        Value rFoo = vm.global("rFoo");
        REQUIRE(rFoo.isInteger());
        REQUIRE(rFoo.toInteger() == 1);
    }

    SECTION("带参数函数调用") {
        sourceFile.load(r, "function add(a, b) { return a + b; } add(1, 2);");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        OptReg retReg{};
        auto chunk = codeGen.parseAst(r, globalNode, retReg);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("add").isObject());

        Value v = vm.curFrame()->getReg(Bytecode::Register{ 0 });
        REQUIRE(v.isInteger());
        REQUIRE(v.toInteger() == 3);
    }

    SECTION("嵌套函数调用") {
        sourceFile.load(r, "function outer(a) { return a; } function inner(b) { return b; } outer(inner(42));");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        OptReg retReg{};
        auto chunk = codeGen.parseAst(r, globalNode, retReg);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("outer").isObject());
        REQUIRE(vm.global("inner").isObject());

        Value v = vm.curFrame()->getReg(Bytecode::Register{ 0 });
        REQUIRE(v.isInteger());
        REQUIRE(v.toInteger() == 42);
    }
}

TEST_CASE("函数声明 - 多个函数定义") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("多个独立函数") {
        sourceFile.load(r, "function a() { return 1; } function b() { return 2; } function c() { return 3; }");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        OptReg retReg{};
        auto chunk = codeGen.parseAst(r, globalNode, retReg);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("a").isObject());
        REQUIRE(vm.global("b").isObject());
        REQUIRE(vm.global("c").isObject());
    }

    SECTION("函数与变量混合") {
        sourceFile.load(r, "var x = 10; function getX() { return x; } function setX(val) { x = val; }");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        OptReg retReg{};
        auto chunk = codeGen.parseAst(r, globalNode, retReg);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();

        Value x = vm.global("x");
        REQUIRE(x.isInteger());
        REQUIRE(x.toInteger() == 10);
        REQUIRE(vm.global("getX").isObject());
        REQUIRE(vm.global("setX").isObject());
    }
}

TEST_CASE("函数声明 - 递归函数") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("直接递归") {
        sourceFile.load(
            r, "function factorial(n) { if (n <= 1) return 1; else return n * factorial(n - 1); } factorial(3);");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        OptReg retReg{};
        auto chunk = codeGen.parseAst(r, globalNode, retReg);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();

        REQUIRE(vm.global("factorial").isObject());
        Value v = vm.curFrame()->getReg(Bytecode::Register{ 0 });
        REQUIRE(v.isInteger());
        REQUIRE(v.toInteger() == 6);
    }
}