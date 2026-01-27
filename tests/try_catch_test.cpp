//
// Created by LiDong on 2026/1/23.
//

#include <catch.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("异常处理 - 执行") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("try内无异常-catch不执行") {
        vm.eval(R"(
            var x = 0;
            try {
                x = 1;
            }
            catch {
                x = 2;
            }
        )"_str);

        REQUIRE(*vm.evalExpr<Integer>("x"_str) == 1);
    }

    SECTION("try内发生异常-catch执行") {
        vm.eval(R"(
            var x = 0;
            try {
                throw "error";
                x = 1; // 不应执行
            }
            catch {
                x = 2;
            }
        )"_str);

        REQUIRE(*vm.evalExpr<Integer>("x"_str) == 2);
    }

    SECTION("异常会中断try剩余代码") {
        vm.eval(R"(
            var x = 0;
            try {
                x = 1;
                throw "boom";
                x = 2; // 不应执行
            }
            catch {
                x = 3;
            }
        )"_str);

        REQUIRE(*vm.evalExpr<Integer>("x"_str) == 3);
    }

    SECTION("catch中再次throw-由上层捕捉") {
        vm.eval(R"(
            var x = 0;

            try {
                try {
                    throw "inner";
                }
                catch(e) {
                    x = 1;
                    throw e;
                }
            }
            catch {
                x = 2;
            }
        )"_str);

        REQUIRE(*vm.evalExpr<Integer>("x"_str) == 2);
    }

    SECTION("异常对象传递") {
        vm.eval(R"(
            var msg = "";
            try {
                throw "hello";
            }
            catch(e) {
                msg = e;
            }
        )"_str);

        REQUIRE(**vm.evalExpr<String *>("msg"_str) == "hello"_str);
    }

    SECTION("未捕捉异常-向VM抛出") {
        REQUIRE_THROWS(vm.eval(R"(
                throw "fatal";
            )"_str));
    }
}
