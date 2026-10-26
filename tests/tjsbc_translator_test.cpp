/*
 * Copyright (c) 2026/10/26
 *
 * TJS2 字节码转译器（阶段 2 / M1）测试：手工构造函数 context 字节码，
 * 转译后经 callValue 直调，验证寄存器恒等映射与算术/返回语义。
 */

#include <catch.hpp>

#include <fmt/ostream.h>

#include "tjsbc/TjsTranslator.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;
using namespace cial::tjsbc;

namespace {
    // f 系列函数 context 构造：slots/code 手工按 tjs2-bytecode.md §5.3/§2 编排
    TjsContext makeFuncContext(Vec<i16> code, const Vec<TjsDataSlot> &slots, const i32 argc) {
        TjsContext ctx{};
        ctx.parent = -1;
        ctx.type = TjsContextType::Function;
        ctx.maxVariableCount = 1;
        ctx.variableReserveCount = 2;
        ctx.maxFrameCount = 2;
        ctx.funcDeclArgCount = argc;
        ctx.propSetter = ctx.propGetter = ctx.superClassGetter = -1;
        ctx.code = std::move(code);
        ctx.data = slots;
        return ctx;
    }
} // namespace

TEST_CASE("TJS2 字节码转译——函数 context 直调（恒等映射）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("f(x) = x * 2") {
        TjsBytecodeFile file{};
        file.topLevel = -1; // 顶层注册模式（global proxy）M2 未支持,只测函数
        file.contexts.push_back(
            makeFuncContext({2, 1, -3, 1, 2, 0, 78, 1, 2, 118, 1, 119},
                            {TjsDataSlot{TJS_TYPE_BYTE, 0}}, 1));
        file.pool.bytes.push_back(2); // CONST 用的 2

        TjsTranslator translator{ rt, file };
        const auto program = translator.translate();
        REQUIRE(program.contexts.size() == 1);
        REQUIRE(program.contexts[0].funcMeta != nullptr);

        // FuncMeta → Function 对象 → callValue(21) → 42
        const Value func = Constant{program.contexts[0].funcMeta}.createValue(&rt);
        REQUIRE(func.isObject());
        Value arg{ Integer{ 21 } };
        const Value ret = vm.callValue(func, 1, &arg);
        REQUIRE(ret.isInteger());
        REQUIRE(ret.asInteger().unwrap() == 42);
    }

    SECTION("多参函数：g(a, b) = a + b * 10") {
        // CP %1,%-3; CP %2,%-4; CONST %3,(BYTE 10); MUL %2,%3; ADD %1,%2; SRV %1; RET
        TjsBytecodeFile file{};
        file.topLevel = -1;
        file.contexts.push_back(
            makeFuncContext({2, 1, -3, 2, 2, -4, 1, 3, 0, 78, 2, 3, 58, 1, 2, 118, 1, 119},
                            {TjsDataSlot{TJS_TYPE_BYTE, 0}}, 2));
        file.pool.bytes.push_back(10);

        TjsTranslator translator{ rt, file };
        const auto program = translator.translate();
        fmt::println("=== g dump ===\n{}", program.contexts[0].chunk->dumpInstructions());
        const Value func = Constant{program.contexts[0].funcMeta}.createValue(&rt);
        Value args[2]{ Value{ Integer{ 3 } }, Value{ Integer{ 4 } } };
        const Value ret = vm.callValue(func, 2, args);
        REQUIRE(ret.isInteger());
        REQUIRE(ret.asInteger().unwrap() == 43); // 3 + 4*10
    }

    SECTION("M1 不支持特性明确报错") {
        // collapse 参数（funcDeclCollapseBase >= 0）
        TjsBytecodeFile file{};
        file.topLevel = -1;
        auto ctx = makeFuncContext({119}, {}, 0);
        ctx.funcDeclCollapseBase = 0;
        file.contexts.push_back(ctx);
        REQUIRE_THROWS(TjsTranslator{ rt, file }.translate());

        // 未知操作码
        TjsBytecodeFile file2{};
        file2.topLevel = -1;
        file2.contexts.push_back(makeFuncContext({200}, {}, 0));
        REQUIRE_THROWS(TjsTranslator{ rt, file2 }.translate());
    }
}
