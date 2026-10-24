//
// Created by LiDong on 2025/12/6.
//

#include <catch.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("循环 - 执行") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("while执行") {
        vm.eval(R"(
            var i = 0;
            while (i < 10) i = i + 1;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("i"_str) == 10);
    }

    SECTION("do-while执行") {
        vm.eval(R"(
            var j = 0; do { j = j + 1; } while (j < 5);
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("j"_str) == 5);
    }

    SECTION("for执行") {
        vm.eval(R"(
            var k = 0; var i; for (i = 0; i < 3; i = i + 1) { k = k + 1; }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("k"_str) == 3);
    }

    SECTION("for首子句var声明") {
        vm.eval(R"(
            var sum = 0;
            for(var i = 0; i < 5; i = i + 1) { sum = sum + i; }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("sum"_str) == 10);
    }

    SECTION("函数内for与后缀递增递减") {
        vm.eval(R"(
            function f() {
                var sum = 0;
                for(var i = 0; i < 5; i++) { sum = sum + i; }
                return sum;
            }
            var r1 = f();

            function g() {
                var n = 5;
                var cnt = 0;
                while(n > 0) { n--; cnt = cnt + 1; }
                return cnt;
            }
            var r2 = g();
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("r1"_str) == 10);
        REQUIRE(*vm.evalExpr<Integer>("r2"_str) == 5);
    }

    SECTION("顶层后缀递减操作数顺序") {
        // 回归：后缀 -- 曾生成 1-n 而非 n-1（++ 因交换律未暴露）
        vm.eval("var a = 5; a--; var b = 5; b++;"_str);
        REQUIRE(*vm.evalExpr<Integer>("a"_str) == 4);
        REQUIRE(*vm.evalExpr<Integer>("b"_str) == 6);
    }

    SECTION("for首子句多变量声明") {
        // tjs.y: for_first_clause -> variable_def_inner -> var variable_id_list
        vm.eval(R"(
            var r = 0;
            for(var i = 0, j = 5; i < j; i = i + 1) { r = r + 1; }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("r"_str) == 5);
    }

    SECTION("for空子句与break") {
        vm.eval(R"(
            var n = 0;
            for(;;) { n = n + 1; if(n == 3) break; }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("n"_str) == 3);
    }

    SECTION("for嵌套与break") {
        vm.eval(R"(
            var count = 0;
            for(var i = 0; i < 3; i++) {
                for(var j = 0; j < 5; j++) {
                    if(j == 2) break;
                    count = count + 1;
                }
            }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("count"_str) == 6);
    }
}
TEST_CASE("循环 - const 声明与 for(const) 首子句") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("const 顶层声明") {
        vm.eval("const K = 42;"_str);
        REQUIRE(*vm.evalExpr<Integer>("K"_str) == 42);
    }

    SECTION("for 首子句 const 声明") {
        vm.eval(R"(
            var sum = 0;
            for(const i = 0; i < 4; i = i + 1) { sum = sum + i; }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("sum"_str) == 6);
    }
}

TEST_CASE("循环 - 未初始化多变量声明相互独立") {
    // 回归:var a, b, c; 曾共享同一 void 寄存器,赋值互相覆盖
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        function f() { var a, b, c; a = 1; b = 2; c = 3; return a + b * 10 + c * 100; }
        var r = f();
        var x, y; x = 7; y = 8;
    )"_str);
    REQUIRE(*vm.evalExpr<Integer>("r"_str) == 321);
    REQUIRE(*vm.evalExpr<Integer>("x"_str) == 7);
    REQUIRE(*vm.evalExpr<Integer>("y"_str) == 8);
}

TEST_CASE("循环 - 后缀 ++/-- 表达式值为旧值（TJS2 语义）") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("局部变量") {
        vm.eval(R"(
            function f() {
                var i = 5;
                var a = i++;
                var b = i--;
                return a * 100 + b * 10 + i;
            }
            var r = f();
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("r"_str) == 565);
    }

    SECTION("全局变量") {
        vm.eval(R"(
            var g = 7;
            var a = g--;
            var b = g++;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("a"_str) == 7);
        REQUIRE(*vm.evalExpr<Integer>("b"_str) == 6);
        REQUIRE(*vm.evalExpr<Integer>("g"_str) == 7);
    }
}
