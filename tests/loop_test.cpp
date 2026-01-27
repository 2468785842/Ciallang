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
}