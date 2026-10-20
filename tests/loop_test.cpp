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
}