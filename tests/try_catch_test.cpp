//
// Created by LiDong on 2026/1/23.
//

#include <catch2/catch_all.hpp>
#include "stdlib/StandardLibrary.hpp"

#include "stdlib/ExceptionLib.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("异常处理 - 未捕获脚本异常向宿主抛出") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    REQUIRE_THROWS(vm.eval(R"(throw "fatal";)"_str));
}

TEST_CASE("Exception 内建类与原生异常包装") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    stdlib::initExceptionClass(vm);

    SECTION("throw new Exception 带 message") {
        vm.eval(R"(
            var m;
            try { throw new Exception("custom"); } catch(e) { m = e.message; }
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("m"_str)) == "custom"_str);
    }

    SECTION("缺省 message") {
        vm.eval(R"(
            var m;
            try { throw new Exception(); } catch(e) { m = e.message; }
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("m"_str)) == "Exception"_str);
    }

    SECTION("原生异常可被 catch 且包装为 Exception") {
        vm.eval(R"(
            var m;
            try { var x = <% 10 %> / <% 5 %>; } catch(e) { m = e.message; }
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("m"_str)).length() > 0);
    }
}

// 回调异常值的哨兵传播（2026-10-30 修正）：宿主回调（sort 比较器/replace 回调/
// 即时求值 EvalStr）里抛出的脚本异常，外层 catch(e) 必须收到抛出值本身。
// 既有缺陷： ThrowHandler::exValueReg==0 被 unwind 判为"无槽位"——catch 变量
// 可能合法落在寄存器 0（或 eval/callValue 哨兵帧的 0 号槽），异常值从未回写，
// catch 读到槽内陈旧值（池内旧对象/void）
TEST_CASE("异常处理 - 回调抛出的异常值完整传播") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("sort 比较器抛出值原样到达 catch（catch 变量在寄存器 0 的布局）") {
        vm.eval(R"(
            var caught;
            try {
                var a = [3, 1, 2];
                a.sort(function(x, y) { throw "cmp-boom"; });
            } catch(e) {
                caught = e;
            }
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("caught"_str)) == "cmp-boom"_str);
    }

    SECTION("String.replace 回调抛出值原样到达 catch") {
        vm.eval(R"(
            var caught2;
            try {
                "aaa".replace(/a/, function(m) { throw "re-boom"; });
            } catch(err) {
                caught2 = err;
            }
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("caught2"_str)) == "re-boom"_str);
    }

    SECTION("即时求值 thrower() ! 的哨兵传播") {
        vm.eval(R"(
            var caught3;
            function thrower() { throw "eval-boom"; }
            try {
                var r = (thrower() !);
            } catch(ex) {
                caught3 = ex;
            }
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("caught3"_str)) == "eval-boom"_str);
    }

    SECTION("无 catch 变量的 try 仍正常拦截") {
        vm.eval(R"(
            var ran = 0;
            try {
                [2, 1].sort(function(x, y) { throw 1; });
                ran = 1;
            } catch {
                ran = 2;
            }
        )"_str);
        REQUIRE(vmState.global("ran").asInteger().unwrap() == 2);
    }
}
