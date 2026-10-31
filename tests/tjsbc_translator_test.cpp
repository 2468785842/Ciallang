/*
 * Copyright (c) 2026/10/26
 *
 * TJS2 字节码转译器（阶段 2）测试：
 * - M1：手工构造函数 context 字节码，转译后经 callValue 直调，验证寄存器恒等映射；
 * - M2：类/NEW/property 闭包/P 变体（参考实现语料端到端，对拍源码路径语义）。
 */

#include <catch2/catch_all.hpp>
#include "stdlib/StandardLibrary.hpp"

#include <fmt/ostream.h>
#include <limits>

#include "stdlib/ExceptionLib.hpp"
#include "stdlib/Print.hpp"
#include "test_config.h"
#include "tjsbc/TjsBytecodeReader.hpp"
#include "tjsbc/TjsTranslator.hpp"
#include "types/Class.hpp"
#include "types/Dictionary.hpp"
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

    // 语料端到端：加载 .tjsbc → 转译 → 执行顶层 context（main.cpp --tjsbc 同款流程，
    // 含 Debug 垫片与 Exception 内建类），返回 VM 供调用方断言全局状态
    struct CorpusRun {
        Runtime rt;
        Context context;
        vm::VMState vmState;
        TjsBytecodeFile file;
        std::unique_ptr<tjsbc::TjsTranslator> translator;
        TjsTranslatedProgram program;

        explicit CorpusRun(const std::string &path) : context(rt), vmState(context) {
            stdlib::registerStandardLibrary(context);
            context.registryGlobalFunc("print"_str, &stdlib::S_PrintFunction);
            context.registryGlobalFunc("println"_str, &stdlib::S_PrintlnFunction);
            VM vm{ &vmState };
            vm.eval(R"(class __DebugShim {
    function __DebugShim() {}
    function message(s) { println(s); }
}
var Debug = new __DebugShim();
)"_str);
            stdlib::initExceptionClass(vm);

            file = readTjsBytecodeFile(path);
            translator = std::make_unique<tjsbc::TjsTranslator>(rt, file);
            program = translator->translate();
            auto *chunk = program.contexts[static_cast<size_t>(program.topLevel)].chunk;
            vmState.allocCallFrame(chunk);
            vmState.makeClosure();
            // 顶层 this = 全局对象（TJS 顶层经 this/proxy 寄存器读写全局）
            vmState.curFrame()->setThisObj(Value{ static_cast<Object *>(vmState.context.global()) });
            try {
                vmState.run();
            } catch(...) {
                vmState.clearException();
                throw;
            }
            vmState.freeCallFrame();
        }
    };
} // namespace

TEST_CASE("TJS2 字节码转译——函数 context 直调（恒等映射）") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("f(x) = x * 2") {
        TjsBytecodeFile file{};
        file.topLevel = -1; // 顶层注册模式（global proxy）M2 未支持,只测函数
        file.contexts.push_back(
            makeFuncContext({ 2, 1, -3, 1, 2, 0, 78, 1, 2, 118, 1, 119 }, { TjsDataSlot{ TJS_TYPE_BYTE, 0 } }, 1));
        file.pool.bytes.push_back(2); // CONST 用的 2

        TjsTranslator translator{ rt, file };
        const auto program = translator.translate();
        REQUIRE(program.contexts.size() == 1);
        REQUIRE(program.contexts[0].funcMeta != nullptr);

        // FuncMeta → Function 对象 → callValue(21) → 42
        const Value func = Constant{ program.contexts[0].funcMeta }.createValue(&rt);
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
        file.contexts.push_back(makeFuncContext({ 2, 1, -3, 2, 2, -4, 1, 3, 0, 78, 2, 3, 58, 1, 2, 118, 1, 119 },
                                                { TjsDataSlot{ TJS_TYPE_BYTE, 0 } }, 2));
        file.pool.bytes.push_back(10);

        TjsTranslator translator{ rt, file };
        const auto program = translator.translate();
        fmt::println("=== g dump ===\n{}", program.contexts[0].chunk->dumpInstructions());
        const Value func = Constant{ program.contexts[0].funcMeta }.createValue(&rt);
        Value args[2]{ Value{ Integer{ 3 } }, Value{ Integer{ 4 } } };
        const Value ret = vm.callValue(func, 2, args);
        REQUIRE(ret.isInteger());
        REQUIRE(ret.asInteger().unwrap() == 43); // 3 + 4*10
    }

    SECTION("CHKINS overwrites target and preserves class name") {
        TjsBytecodeFile file{};
        file.topLevel = -1;
        file.pool.strings.push_back("Integer"_str);
        // CP r1,arg0; CONST r2,"Integer"; CHKINS r1,r2; SRV r1; RET
        file.contexts.push_back(
            makeFuncContext({ 2, 1, -3, 1, 2, 0, 88, 1, 2, 118, 1, 119 }, { TjsDataSlot{ TJS_TYPE_STRING, 0 } }, 1));
        TjsTranslator translator{ rt, file };
        const auto program = translator.translate();
        const Value func = Constant{ program.contexts[0].funcMeta }.createValue(&rt);
        for(const auto &arg : { Value{ Integer{ 21 } }, Value{ "text"_str } }) {
            const Value ret = vm.callValue(func, 1, &arg);
            REQUIRE(ret.isInteger());
            REQUIRE(ret.asInteger().unwrap() == (arg.isInteger() ? 1 : 0));
        }
    }

    SECTION("未知操作码明确报错") {
        TjsBytecodeFile file2{};
        file2.topLevel = -1;
        file2.contexts.push_back(makeFuncContext({ 200 }, {}, 0));
        REQUIRE_THROWS(TjsTranslator{ rt, file2 }.translate());
    }
}

// ──────────────────────────── M2：类/property 闭包/P 变体 ────────────────────────────

TEST_CASE("TJS2 字节码转译——M2 语料端到端（类/NEW/property/继承）") {
    SECTION("s6_class：类 + NEW + 成员 property + &/* 闭包 + P 变体") {
        CorpusRun run{ TEST_FILES_PATH R"(/tjsbc/s6_class.tjsbc)" };
        // 序列：4 → 6 → 8(*f) → 9(++) → 11(*f += 2)，实例槽最终 legs==11
        const Value a = run.vmState.context.global()->getProp(run.rt.atomTable.intern("a"_str));
        REQUIRE(a.isObject());
        const Value legs = a.asObject().unwrap()->getProp(run.rt.atomTable.intern("legs"_str));
        REQUIRE(legs.isInteger());
        REQUIRE(legs.asInteger().unwrap() == 11);
    }

    SECTION("s7_ext：extends + super.<Ctor>() + super.<方法>()") {
        CorpusRun run{ TEST_FILES_PATH R"(/tjsbc/s7_ext.tjsbc)" };
        const Value d = run.vmState.context.global()->getProp(run.rt.atomTable.intern("d"_str));
        REQUIRE(d.isObject());
        // Dog 实例经继承初始化了基类 var 成员（Animal(4) 构造写入）
        const Value legs = d.asObject().unwrap()->getProp(run.rt.atomTable.intern("legs"_str));
        REQUIRE(legs.isInteger());
        REQUIRE(legs.asInteger().unwrap() == 4);
        // 基类已挂到实例 super 链（ClassObject::call 的 setSuperClass）
        auto *dataObj = dynamic_cast<DataObject *>(d.asObject().unwrap());
        REQUIRE(dataObj != nullptr);
        REQUIRE(dataObj->getSuperClass(run.rt.atomTable.intern("Animal"_str)) != nullptr);
    }

    SECTION("test_class：kirikiri 原版类测试全量（property/闭包/继承/invalidate）") {
        CorpusRun run{ TEST_FILES_PATH R"(/tjsbc/test_class.tjsbc)" };
        // 脚本全链路断言内建（不等式失败即 throw），走到尾即通过；
        // 这里抽查 classproptest 经 property 闭包链写入的全局 propvalue==20
        const Value pv = run.vmState.context.global()->getProp(run.rt.atomTable.intern("propvalue"_str));
        REQUIRE(pv.isInteger());
        REQUIRE(pv.asInteger().unwrap() == 20);
    }

    SECTION("test_function：省略透传/展开调用/collapse 参数（M4）") {
        // kirikiri 原版函数测试全量：成功时零输出（失败即 throw），走到尾即通过。
        // 源码路径不支持 `...`/`*` 语法，对拍基准为原版引擎行为（成功 = 无异常）
        CorpusRun run{ TEST_FILES_PATH R"(/tjsbc/test_function.tjsbc)" };
        // 抽查：argMul(*argsArray) 展开调用经 collapse 参数往返后全局写入的标记
        // （脚本尾部 isFaild 应为 false）
        const Value isFaild = run.vmState.context.global()->getProp(run.rt.atomTable.intern("isFaild"_str));
        REQUIRE(isFaild.isInteger());
        REQUIRE(isFaild.asInteger().unwrap() == 0);
    }
}

TEST_CASE("TJS2 字节码转译——TYPEOFD/DELD 直译") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // f(o) = { typeof o.x; delete o.x } → 返回删除是否成功(bool)
    TjsBytecodeFile file{};
    file.topLevel = -1;
    file.pool.strings.push_back("x"_str);
    file.contexts.push_back(makeFuncContext(
        // TYPEOFD r1,-3,0 (丢结果) ; DELD r1,-3,0 ; SRV r1 ; RET
        { 84, 1, -3, 0, 116, 1, -3, 0, 118, 1, 119 }, { TjsDataSlot{ TJS_TYPE_STRING, 0 } }, 1));

    TjsTranslator translator{ rt, file };
    const auto program = translator.translate();
    const Value func = Constant{ program.contexts[0].funcMeta }.createValue(&rt);

    Value dict = Value{ rt.create<Dictionary>().get() };
    dict.asObject().unwrap()->setProp(rt.atomTable.intern("x"_str), Value{ Integer{ 1 } });
    REQUIRE(dict.isObject());
    Value args[1]{ dict };
    const Value ret = vm.callValue(func, 1, args);
    REQUIRE(ret.isInteger());
    REQUIRE(ret.asInteger().unwrap() != 0); // 删除成功 → true
    // 成员已被删除
    REQUIRE(dict.asObject().unwrap()->hasProp(rt.atomTable.intern("x"_str)) == false);
}

TEST_CASE("TJS2 字节码转译——BNOT/NUM/OCTET 转换指令（M3）") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("BNOT：~5 → -6") {
        // CONST r1 ← BYTE(5) ; BNOT r1,r1 ; SRV r1 ; RET
        TjsBytecodeFile file{};
        file.topLevel = -1;
        file.pool.bytes.push_back(5);
        file.contexts.push_back(
            makeFuncContext({ 1, 1, 0, 82, 1, 118, 1, 119 }, { TjsDataSlot{ TJS_TYPE_BYTE, 0 } }, 1));
        TjsTranslator translator{ rt, file }; // 具名持有：GC 根须存活至断言结束
        const auto program = translator.translate();
        const Value func = Constant{ program.contexts[0].funcMeta }.createValue(&rt);
        Value arg{ Integer{ 5 } };
        const Value ret = vm.callValue(func, 1, &arg);
        REQUIRE(ret.isInteger());
        REQUIRE(ret.asInteger().unwrap() == ~5);
    }

    SECTION("NUM：字符串转 42") {
        // CONST r1 ← STR ; NUM r1,r1 ; SRV r1 ; RET
        TjsBytecodeFile file{};
        file.topLevel = -1;
        file.pool.strings.push_back("42"_str);
        file.contexts.push_back(
            makeFuncContext({ 1, 1, 0, 91, 1, 118, 1, 119 }, { TjsDataSlot{ TJS_TYPE_STRING, 0 } }, 1));
        TjsTranslator translator{ rt, file };
        const auto program = translator.translate();
        const Value func = Constant{ program.contexts[0].funcMeta }.createValue(&rt);
        Value arg = Value{ "42"_str };
        const Value ret = vm.callValue(func, 1, &arg);
        REQUIRE(ret.isInteger());
        REQUIRE(ret.asInteger().unwrap() == 42);
    }

    SECTION("OCTET：void → 空 octet") {
        // CL r1 ; OCTET r1,r1 ; SRV r1 ; RET
        TjsBytecodeFile file{};
        file.topLevel = -1;
        file.contexts.push_back(makeFuncContext({ 3, 1, 98, 1, 118, 1, 119 }, {}, 1));
        TjsTranslator translator{ rt, file };
        const auto program = translator.translate();
        const Value func = Constant{ program.contexts[0].funcMeta }.createValue(&rt);
        Value arg{};
        const Value ret = vm.callValue(func, 1, &arg);
        REQUIRE(ret.isOctet());
        REQUIRE(ret.asOctet().unwrap()->getSize() == 0);
    }
}

TEST_CASE("TJS2 translator rejects truncated and invalid instruction operands", "[tjsbc][validation]") {
    const Vec<Vec<i16>> invalidCode{
        { 1 }, // CONST 缺少寄存器/常量槽
        { 99, 1, 2, -3 }, // CALL 非法负实参数量
        { 99, 1, 2, -2, -1 }, // CALL 展开表负条目数
        { 99, 1, 2, 2, 0 }, // CALL 两个实参只存一个
        { 100, 1, 2, 0, -2, 1, 0 }, // CALLD 展开条目缺少寄存器
        { 1, 1, -1, 119 }, // CONST 负常量槽
        { 17, -1 }, // JMP 指向代码之前
        { 120, 32767, 1 }, // ENTRY catch 超出代码
    };
    for(size_t i = 0; i < invalidCode.size(); ++i) {
        INFO("invalid instruction case " << i);
        Runtime rt;
        TjsBytecodeFile file;
        file.contexts.push_back(makeFuncContext(invalidCode[i], {}, 0));
        TjsTranslator translator{ rt, file };
        REQUIRE_THROWS_WITH(translator.translate(), Catch::Matchers::ContainsSubstring("tjs translate:"));
    }
}

TEST_CASE("TJS2 class loading validates code and superclass references", "[tjsbc][validation]") {
    const Vec<Vec<i16>> invalidCode{
        { 1, 1 }, // 截断 CONST
        { 1, 1, 0 }, // CONST 指向不存在的 DataArea
        { 111, -1, -1, 0 }, // 成员名负下标
        { 118 }, // 截断 SRV
        { 103, 1, -2, 0, 123, 1, -1, 99, 0, 1, -2 }, // 非法 super CALL 实参数量
    };
    for(size_t i = 0; i < invalidCode.size(); ++i) {
        INFO("invalid class case " << i);
        Runtime rt;
        TjsBytecodeFile file;
        file.pool.strings.push_back("Broken"_str);
        TjsContext context;
        context.type = TjsContextType::Class;
        context.name = 0;
        context.code = invalidCode[i];
        file.contexts.push_back(std::move(context));
        TjsTranslator translator{ rt, file };
        REQUIRE_THROWS_WITH(translator.translate(), Catch::Matchers::ContainsSubstring("tjs translate:"));
    }

    SECTION("superclass context index must exist") {
        Runtime rt;
        TjsBytecodeFile file;
        file.pool.strings.push_back("Broken"_str);
        TjsContext context;
        context.type = TjsContextType::Class;
        context.name = 0;
        context.superClassGetter = 1;
        file.contexts.push_back(context);
        TjsTranslator translator{ rt, file };
        REQUIRE_THROWS_WITH(translator.translate(), Catch::Matchers::ContainsSubstring("context index out of range"));
    }
    SECTION("superclass code pointer uses checked unsigned bounds") {
        Runtime rt;
        TjsBytecodeFile file;
        file.pool.strings.push_back("Broken"_str);
        TjsContext context;
        context.type = TjsContextType::Class;
        context.name = 0;
        context.superClassGetter = 1;
        file.contexts.push_back(context);
        TjsContext getter;
        getter.type = TjsContextType::SuperClassGetter;
        getter.superClassGetterPointers.push_back(std::numeric_limits<i32>::max());
        file.contexts.push_back(getter);
        TjsTranslator translator{ rt, file };
        REQUIRE_THROWS_WITH(translator.translate(), Catch::Matchers::ContainsSubstring("unsupported pattern"));
    }
}

TEST_CASE("TJS2 fixed width members do not read a trailing extra word", "[tjsbc][validation]") {
    for(const Vec<i16> &code : { Vec<i16>{ 103, 1, -2, 0 }, Vec<i16>{ 19, 1, -2, 0 } }) {
        Runtime rt;
        TjsBytecodeFile file;
        file.pool.strings.push_back("field"_str);
        file.contexts.push_back(makeFuncContext(code, { { TJS_TYPE_STRING, 0 } }, 0));
        TjsTranslator translator{ rt, file };
        REQUIRE_NOTHROW(translator.translate());
    }
}

TEST_CASE("TJS2 MUL direct member variant updates and returns its result", "[tjsbc][validation]") {
    Runtime rt;
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
    vm::VMState state{ context };
    VM vm{ &state };
    TjsBytecodeFile file;
    file.pool.bytes.push_back(3);
    file.pool.strings.push_back("field"_str);
    // CONST r1,3; MULPD r2,arg0,*field,r1; SRV r2; RET
    file.contexts.push_back(makeFuncContext({ 1, 1, 0, 79, 2, -3, 1, 1, 118, 2, 119 },
                                            { { TJS_TYPE_BYTE, 0 }, { TJS_TYPE_STRING, 0 } }, 1));
    TjsTranslator translator{ rt, file };
    const auto program = translator.translate();
    auto function = rt.create<Function>(program.contexts[0].funcMeta);
    auto dictionary = rt.create<Dictionary>();
    const Atom field = rt.atomTable.intern("field"_str);
    dictionary->setProp(field, Value{ Integer{ 7 } });
    Value argument{ dictionary.get() };
    REQUIRE(vm.callValue(Value{ function.get() }, 1, &argument).asInteger().unwrap() == 21);
    REQUIRE(dictionary->getProp(field).asInteger().unwrap() == 21);
}
