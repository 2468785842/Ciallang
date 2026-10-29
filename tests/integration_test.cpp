/*
 * Copyright (c) 2024/12/18
 *
 * 集成测试 - 完整脚本执行流程
 * 源码 → Parser → IRGenerator → run0 扁平字节码 → VM，多特性组合场景
 * 经 VM 门面端到端执行后断言全局状态（与 interpreter_test 的单特性/错误路径互补）。
 */

#include <catch.hpp>

#include "stdlib/ExceptionLib.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("集成 - 函数与控制流组合") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        function fib(n) {
            if (n < 2) return n;
            return fib(n - 1) + fib(n - 2);
        }
        function sum(a, b = 10) { return a + b; }
        var r1 = fib(10);
        var r2 = sum(5);
        var total = 0;
        for (var i = 1; i <= 10; i++) {
            if (i % 2 == 0) continue;
            total += i;
        }
        var w = 0;
        var k = 0;
        while (k < 3) { k++; w += k; }
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("r1"_str) == 55);
    REQUIRE(*vm.evalExpr<Integer>("r2"_str) == 15);
    REQUIRE(*vm.evalExpr<Integer>("total"_str) == 25);
    REQUIRE(*vm.evalExpr<Integer>("w"_str) == 6);
}

TEST_CASE("集成 - 容器与字符串组合") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        var arr = [3, 1, 2];
        arr.push(4);
        arr.sort();
        var s = "";
        for (var i = 0; i < arr.count; i++) {
            s += arr[i] + ",";
        }
        var d = %["a" >= 1, "b" >= 2];
        d["c"] = 3;
        var dc = d.count;
        var up = "abc".toUpperCase();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("arr[0]"_str) == 1);
    REQUIRE(*vm.evalExpr<Integer>("arr[3]"_str) == 4);
    REQUIRE(*(*vm.evalExpr<String *>("s"_str)) == "1,2,3,4,"_str);
    REQUIRE(*vm.evalExpr<Integer>("dc"_str) == 3);
    REQUIRE(*vm.evalExpr<Integer>("d[\"c\"]"_str) == 3);
    REQUIRE(*(*vm.evalExpr<String *>("up"_str)) == "ABC"_str);
}

TEST_CASE("集成 - 类、继承、property 与异常组合") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    stdlib::initExceptionClass(vm);

    vm.eval(R"(
        class Animal {
            function speak() { return "..."; }
        }
        class Dog extends Animal {
            property tag { getter() { return "dog"; } }
            function speak() { return "woof"; }
            function describe() { return tag + " says " + this.speak(); }
        }
        var d = new Dog();
        var sp = d.speak();
        var de = d.describe();
        var caught = 0;
        var msg = "";
        try {
            throw new Exception("boom");
        } catch(e) {
            caught = 1;
            msg = e.message;
        }
        var after = 1;
    )"_str);

    REQUIRE(*(*vm.evalExpr<String *>("sp"_str)) == "woof"_str);
    REQUIRE(*(*vm.evalExpr<String *>("de"_str)) == "dog says woof"_str);
    REQUIRE(*vm.evalExpr<Integer>("caught"_str) == 1);
    REQUIRE(*(*vm.evalExpr<String *>("msg"_str)) == "boom"_str);
    REQUIRE(*vm.evalExpr<Integer>("after"_str) == 1);
}

TEST_CASE("集成 - with 语句写实例成员") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        var obj = %["x" >= 0, "y" >= 0];
        with (obj) {
            .x = 42;
            .y = .x + 1;
        }
        var x = obj.x;
        var y = obj.y;
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("x"_str) == 42);
    REQUIRE(*vm.evalExpr<Integer>("y"_str) == 43);
}

TEST_CASE("集成 - 同一 VM 跨脚本共享全局与函数") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval("var acc = 0; function add(n) { acc += n; return acc; }"_str);
    vm.eval("add(2);"_str);
    vm.eval("add(3);"_str);

    REQUIRE(*vm.evalExpr<Integer>("acc"_str) == 5);
    REQUIRE(*vm.evalExpr<Integer>("add(4)"_str) == 9);
}
