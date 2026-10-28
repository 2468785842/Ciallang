/*
 * Copyright (c) 2024/5/30 上午9:14
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */

#include <catch.hpp>

#include "stdlib/ExceptionLib.hpp"
#include "stdlib/Print.hpp"
#include "test_config.h"

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

#include "types/Function.hpp"

using namespace cial;

TEST_CASE("解释器 - Hello World") {
    Runtime rt{};
    Context context{ rt };
    context.registryGlobalFunc("println"_str, &stdlib::S_PrintlnFunction);
    vm::VMState vmState{ context };
    const VM vm{ &vmState };
    vm.evalExpr(R"(println("Hello World!"))"_str);
}

TEST_CASE("解释器 - 执行测试") {
    Runtime rt{};
    Context context{ rt };
    context.registryGlobalFunc("println"_str, &stdlib::S_PrintlnFunction);
    vm::VMState vmState{ context };
    const VM vm{ &vmState };
    vm.eval(String(TEST_FILES_PATH R"(/startup.tjs)"), true);
}

TEST_CASE("解释器 - 边界与错误路径") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    const VM vm{ &vmState };

    SECTION("空脚本与纯空白脚本不崩溃") {
        // 回归：空脚本曾在 Release 下越过 assert 段错误
        REQUIRE_NOTHROW(vm.eval(""_str));
        REQUIRE_NOTHROW(vm.eval("   "_str));
    }

    SECTION("无尾换行脚本后继续evalExpr") {
        // 回归：EOF 读取曾推进 pos 越过缓冲区，无换行源 + 回溯 = 越界
        vm.eval("var a = 42;"_str);
        REQUIRE(*vm.evalExpr<Integer>("a"_str) == 42);

        // 无分号无换行：标识符恰好结束于 EOF
        vm.eval("var b;"_str);
        REQUIRE(vm.evalExpr<Value>("b"_str)->isVoid());
    }

    SECTION("非法语法优雅失败") {
        // 回归：解析失败曾在 Release 下段错误（assert 兜底不存在）
        REQUIRE_NOTHROW(vm.eval("~1;"_str));
        REQUIRE_NOTHROW(vm.eval("var ;"_str));
        REQUIRE_NOTHROW(vm.eval("for("_str));

        // 失败后 VM 状态不被破坏，后续求值正常
        REQUIRE_NOTHROW(vm.eval("var ok = 7;"_str));
        REQUIRE(*vm.evalExpr<Integer>("ok"_str) == 7);
    }

    SECTION("evalExpr空表达式") {
        REQUIRE_NOTHROW(vm.evalExpr(""_str));
    }

    SECTION("函数体含不支持语法不崩溃") {
        // 回归:前缀 &/*/未支持时 generateFuncMeta 曾对空 Opt 解引用段错误
        // (顶层 chunk 有判空,函数级只有 assert,Release 下直接崩)
        REQUIRE_NOTHROW(vm.eval("function f() { var p = 1; *p = 30; }"_str));
        REQUIRE_NOTHROW(vm.eval("function g() { var p = &property1; }"_str));
        // 运行期未捕获异常:unwind 抛 C++ 异常穿过 eval(由宿主捕获,见 main.cpp)
        REQUIRE_THROWS_AS(vm.eval("throw \"boom\";"_str), std::exception);
        // 失败后状态未破坏,后续求值正常
        REQUIRE(*vm.evalExpr<Integer>("40 + 2"_str) == 42);
    }
}

TEST_CASE("with 语句与裸词链") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    stdlib::initExceptionClass(vm);

    SECTION("with(obj) 块内裸词链读写与调用") {
        vm.eval(R"(
            class C {
                var a, b;
                function C() { a = b = 0; }
                function bump() { a = a + 1; }
            }
            var o = new C();
            with(o) {
                .a = 5;
                .b = .a + 1;
                .bump();
            }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("o.a"_str) == 6);
        REQUIRE(*vm.evalExpr<Integer>("o.b"_str) == 6);
    }

    SECTION("with 单语句形式") {
        vm.eval(R"(
            var o = %[x : 0];
            with(o) .x = 7;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("o.x"_str) == 7);
    }

    SECTION("嵌套 with 栈顶优先") {
        vm.eval(R"(
            var d1 = %[v : 1];
            var d2 = %[v : 2];
            var hit1, hit2;
            with(d1) {
                with(d2) {
                    hit2 = .v;
                }
                hit1 = .v;
            }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("hit1"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("hit2"_str) == 2);
    }

    SECTION("顶层裸词链写全局") {
        vm.eval(R"(
            .gvarl = 7;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("gvarl"_str) == 7);
    }

    SECTION("with 对象可为表达式且每次求值") {
        vm.eval(R"(
            var d = %[v : 3];
            var r;
            with(d) .v = .v + 1;
            r = d.v;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("r"_str) == 4);
    }
}
