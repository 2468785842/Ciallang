/*
 * Copyright (c) 2024/12/18
 *
 * 函数声明单元测试
 */

#include <catch.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

#include "types/Function.hpp"

using namespace cial;

TEST_CASE("函数声明 - 简单函数定义") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        function empty() { }
        function foo() { return 42; }
        function add(a, b) { return a + b; }
    )"_str);

    REQUIRE(*vm.getGlobal<Object *>("empty"_str));
    REQUIRE(*vm.getGlobal<Object *>("foo"_str));
    REQUIRE(*vm.getGlobal<Object *>("add"_str));

    REQUIRE(vm.evalExpr<Value>("empty()"_str)->isVoid());
    REQUIRE(*vm.evalExpr<Integer>("foo()"_str) == 42);
    REQUIRE(*vm.evalExpr<Integer>("add(1, 2)"_str) == 3);
}

TEST_CASE("函数声明 - 参数列表") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        function single(x) { return x; }
        var rSingle = single(1);
        function multi(a, b, c) { return a + b + c; }
        var rMulti = multi(1, 2, 3);
        function complex(param1, param_2, param3) { return param1; }
        var rComplex1 = complex();
        var rComplex2 = complex(1);
        var rComplex3 = complex(1, 2, 3);
    )"_str);

    REQUIRE(*vm.getGlobal<Function *>("single"_str));
    REQUIRE(*vm.getGlobal<Function *>("multi"_str));
    REQUIRE(*vm.getGlobal<Function *>("complex"_str));

    REQUIRE(*vm.getGlobal<Integer>("rSingle"_str) == 1);
    REQUIRE(*vm.getGlobal<Integer>("rMulti"_str) == 6);
    REQUIRE(vm.getGlobal<Value>("rComplex1"_str)->isVoid());
    REQUIRE(*vm.getGlobal<Integer>("rComplex2"_str) == 1);
    REQUIRE(*vm.getGlobal<Integer>("rComplex3"_str) == 1);
}

TEST_CASE("函数声明 - 函数体内容") {

    SECTION("带变量声明的函数体") {

        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        vm.eval(R"(
            function test() {
                var x = 10;
                var y = 20;
                return x + y;
            }
            var rTest = test();
        )"_str);

        REQUIRE(*vm.getGlobal<Function *>("test"_str));
        REQUIRE(*vm.evalExpr<Integer>("rTest"_str) == 30);
    }

    SECTION("带控制流的函数体") {

        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        vm.eval(R"(
            function control() {
                if (true) return 1;
                else return 0;
            }
            var rControl = control();
        )"_str);

        REQUIRE(*vm.getGlobal<Function *>("control"_str));
        REQUIRE(*vm.evalExpr<Integer>("rControl"_str) == 1);
    }

    SECTION("带循环的函数体") {

        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        vm.eval(R"(
            function loop() {
                var i = 0;
                while (i < 10) i = i + 1;
                return i;
            }
            var rLoop = loop();
        )"_str);

        REQUIRE(*vm.getGlobal<Function *>("loop"_str));
        REQUIRE(*vm.evalExpr<Integer>("rLoop"_str) == 10);
    }
}

TEST_CASE("函数声明 - 函数调用") {

    SECTION("无参数省略括号函数调用") {
        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        vm.eval(R"(
            function foo { return 1; }
            var rFoo = foo();
        )"_str);
        REQUIRE(*vm.getGlobal<Function *>("foo"_str));
        REQUIRE(*vm.evalExpr<Integer>("rFoo"_str) == 1);
    }

    SECTION("无参数函数调用") {
        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        vm.eval(R"(
            function foo() { return 1; }
            var rFoo = foo();
        )"_str);
        REQUIRE(*vm.getGlobal<Function *>("foo"_str));
        REQUIRE(*vm.evalExpr<Integer>("rFoo"_str) == 1);
    }

    SECTION("带参数函数调用") {
        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        VM::Handle<Value> ret = vm.eval<Value>(R"(
            function add(a, b) { return a + b; }
        )"_str);
        REQUIRE(*vm.getGlobal<Function *>("add"_str));
        REQUIRE(*vm.evalExpr<Integer>("add(1, 2)"_str) == 3);
    }

    SECTION("嵌套函数调用") {
        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        VM::Handle<Value> ret = vm.eval<Value>(R"(
            function outer(a) { return a; }
            function inner(b) { return b; }
        )"_str);
        REQUIRE(*vm.getGlobal<Function *>("outer"_str));
        REQUIRE(*vm.getGlobal<Function *>("inner"_str));
        REQUIRE(*vm.evalExpr<Integer>("outer(inner(42))"_str) == 42);
    }
}

TEST_CASE("函数声明 - 多个函数定义") {

    SECTION("多个独立函数") {

        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        VM::Handle<Value> ret = vm.eval<Value>(R"(
            function a() { return 1; }
            function b() { return 2; }
            function c() { return 3; }
        )"_str);
        REQUIRE(*vm.getGlobal<Function *>("a"_str));
        REQUIRE(*vm.getGlobal<Function *>("b"_str));
        REQUIRE(*vm.getGlobal<Function *>("c"_str));
        REQUIRE(*vm.evalExpr<Integer>("a()"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("b()"_str) == 2);
        REQUIRE(*vm.evalExpr<Integer>("c()"_str) == 3);
    }

    SECTION("函数与变量混合") {

        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        VM::Handle<Value> ret = vm.eval<Value>(R"(
            var x = 10;
            function getX() { return x; }
            function setX(val) { x = val; }
        )"_str);
        REQUIRE(*vm.getGlobal<Function *>("getX"_str));
        REQUIRE(*vm.getGlobal<Function *>("setX"_str));
        REQUIRE(*vm.evalExpr<Integer>("x"_str) == 10);
        REQUIRE(*vm.evalExpr<Integer>("getX()"_str) == 10);
        REQUIRE(*vm.eval<Integer>("setX(20); return getX();"_str) == 20);
    }
}

TEST_CASE("函数声明 - 递归函数") {

    SECTION("直接递归") {

        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        VM::Handle<Integer> ret = vm.eval<Integer>(R"(
            function factorial(n) {
                if (n <= 1) return 1;
                else return n * factorial(n - 1);
            }
            return factorial(3);
        )"_str);
        REQUIRE(*vm.getGlobal<Function *>("factorial"_str));
        REQUIRE(*ret == 6);
    }
}

TEST_CASE("省略透传 / collapse 参数 / 展开调用（源码路径，对齐 tjs.y func_decl_arg_collapse）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("`...` 省略透传（旧版 argc=-1）") {
        vm.eval(R"(
            function argMul(a, b) { return a * b; }
            function argThrough() { return argMul(...); }
            var r = argThrough(5, 3);
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("r"_str) == 15);
    }

    SECTION("`name*` collapse 参数：超额实参收拢成 Array") {
        vm.eval(R"(
            function arrayArg(a*) { return a[0] + a[1] * 10; }
            var r = arrayArg(2, 7);
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("r"_str) == 72);
    }

    SECTION("`expr*` 展开调用与混合形态") {
        vm.eval(R"(
            function sum3(a, b, c) { return a + b * 100 + c * 10000; }
            var arr = [2];
            var r1 = sum3(1, arr*, 4);
            var args = [1, 2, 3];
            var r2 = sum3(args*);
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("r1"_str) == 1 + 2 * 100 + 4 * 10000);
        REQUIRE(*vm.evalExpr<Integer>("r2"_str) == 1 + 2 * 100 + 3 * 10000);
    }

    SECTION("裸 `*` 未命名收拢/展开对") {
        vm.eval(R"(
            function wrap(*) { return passthrough(*); }
            function passthrough(a, b) { return a + b; }
            var r = wrap(30, 12);
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("r"_str) == 42);
    }
}
