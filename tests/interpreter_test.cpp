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

#include "StderrCapture.hpp"

#include "stdlib/ExceptionLib.hpp"
#include "stdlib/Print.hpp"
#include "test_config.h"

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

#include "types/Class.hpp"
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
        test::StderrCapture diagnostic;
        REQUIRE_NOTHROW(vm.eval("   "_str));
        REQUIRE(diagnostic.finish() == "cial: error: script produced no code\n");
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
        for(const auto &source : { "var ;"_str, "for("_str }) {
            test::StderrCapture diagnostic;
            const auto result = vm.eval<Value>(source);
            const auto error = diagnostic.finish();
            REQUIRE(result.get().isVoid());
            REQUIRE(error == "cial: error: script produced no code\n");
        }

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

TEST_CASE("Global slot cache observes replacement, deletion and reinsertion", "[cache]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval("function f() { return 1; } function call() { return f(); } var first = call();"_str);
    vm.eval("function replacement() { return 2; } f = replacement; var second = call();"_str);
    REQUIRE(state.global("first").asInteger().unwrap() == 1);
    REQUIRE(state.global("second").asInteger().unwrap() == 2);
    const Atom f = rt.atomTable.intern("f"_str);
    REQUIRE(context.global()->delProp(f));
    REQUIRE_THROWS(vm.eval("call();"_str));
    context.global()->setProp(f, state.global("replacement"));
    REQUIRE(*vm.evalExpr<Integer>("call()"_str) == 2);
}

TEST_CASE("GC finalizers run only in their owning VM", "[gc]") {
    Runtime rt;
    Context first{ rt }, second{ rt };
    vm::VMState stateA{ first }, stateB{ second };
    VM a{ &stateA }, b{ &stateB };
    const auto script = R"(
        var finalized = 0;
        class A {
            function A() {}
            function finalize() { finalized++; }
        }
        var obj = new A(); obj = void;
    )"_str;
    a.eval(script);
    b.eval(script);
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    a.eval("var done = finalized;"_str);
    REQUIRE(stateA.global("done").asInteger().unwrap() == 1);
    REQUIRE(stateB.global("finalized").asInteger().unwrap() == 0);
    REQUIRE(rt.markSweep.pendingFinalizers.size() == 1);
    b.eval("var done = finalized;"_str);
    REQUIRE(stateB.global("done").asInteger().unwrap() == 1);
    REQUIRE(rt.markSweep.pendingFinalizers.empty());
}

TEST_CASE("GC automatic collection drains finalizers without growing the VM stack", "[gc]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval(R"(
        var finalized = 0;
        class A {
            function A() {}
            function finalize() { finalized++; }
        }
        function drop() { var a = new A(); }
        for(var i = 0; i < 1500; i++) drop();
    )"_str);
    REQUIRE(state.global("finalized").asInteger().unwrap() > 0);
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    vm.eval("var total = finalized;"_str);
    REQUIRE(state.global("total").asInteger().unwrap() == 1500);
    REQUIRE(context.stackTop() == 0);
    REQUIRE(state.getRegPoolTop() == 0);
}

TEST_CASE("GC finalizers run at VM safe points and retain cyclic dependencies", "[gc]") {
    Runtime rt{ 512 };
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval(R"(
        var finalized = 0;
        var resurrected;
        class A {
            var other;
            var x = 7;
            function A() {}
            function finalize() {
                finalized += other.x;
                resurrected = this;
                // 回调内部再次回收，队列和对象依赖必须仍然存活
                gcCollect();
            }
        }
        var a = new A();
        var b = new A();
        a.other = b; b.other = a;
        a = void; b = void;
    )"_str);
    auto collect =
        rt.create<NativeFunction>([&](VM *, Value, size_t) { rt.markSweep.collect([&] { rt.collectAllMark(); }); });
    context.registryGlobalFunc("gcCollect"_str, collect.get());
    const auto before = state.getRegPoolTop();
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    REQUIRE(rt.markSweep.pendingFinalizers.size() == 2);
    REQUIRE(state.global("finalized").asInteger().unwrap() == 0);
    vm.eval("var afterDrain = finalized;"_str);
    REQUIRE(state.global("afterDrain").asInteger().unwrap() == 14);
    REQUIRE(rt.markSweep.pendingFinalizers.empty());
    REQUIRE(state.getRegPoolTop() == before);
    REQUIRE(context.stackTop() == 0);
    vm.eval("resurrected = void;"_str);
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    REQUIRE(rt.markSweep.pendingFinalizers.empty());
}

TEST_CASE("GC finalizers are once-only and exceptions restore the VM", "[gc]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval(R"(
        var count = 0;
        class A {
            function A() {}
            function finalize() { count++; }
        }
        var a = new A(); invalidate a; a = void;
    )"_str);
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    REQUIRE(rt.markSweep.pendingFinalizers.empty());
    REQUIRE(state.global("count").asInteger().unwrap() == 1);
    vm.eval(R"(
        class B {
            function B() {}
            function finalize() { throw "finalizer failure"; }
        }
        var b = new B(); b = void;
    )"_str);
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    REQUIRE(rt.markSweep.pendingFinalizers.size() == 1);
    REQUIRE_THROWS(vm.eval("var untouched = 1;"_str));
    REQUIRE_FALSE(rt.markSweep.drainingFinalizers);
    REQUIRE(rt.markSweep.pendingFinalizers.empty());
    REQUIRE(*vm.evalExpr<Integer>("40 + 2"_str) == 42);
}

TEST_CASE("GC ignores synthetic empty finalizers", "[gc]") {
    Runtime rt;
    Context context{ rt };
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval(R"(
        class A { function A() {} }
        var a = new A(); a.finalize(); invalidate a; a = void;
        var b = new A(); b = void;
    )"_str);
    rt.markSweep.collect([&] { rt.collectAllMark(); });
    REQUIRE(rt.markSweep.pendingFinalizers.empty());
    REQUIRE(context.stackTop() == 0);
    REQUIRE(state.getRegPoolTop() == 0);
    struct Probe final : MarkSweepHeader {
        int *count;
        explicit Probe(int *counter) : count(counter) {}
        ~Probe() override { ++*count; }
    };
    int destroyed = 0;
    {
        Runtime owner{ 8 };
        { auto probe = owner.create<Probe>(&destroyed); }
        owner.markSweep.collect([&] { owner.collectAllMark(); });
        REQUIRE(destroyed == 1);
        { auto probe = owner.create<Probe>(&destroyed); }
    }
    REQUIRE(destroyed == 2);
}
