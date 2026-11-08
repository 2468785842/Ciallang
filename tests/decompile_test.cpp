/*
 * Copyright (c) 2026/11/1
 *
 * 反编译器 M0 测试（doc/bytecode/decompiler-plan.md）：
 * - 自产路径 {文本, 行表}：TAC 半开区间完备覆盖、synthetic 标记、严格状态；
 * - TJS2 路径来源表：TacOrigin 码字坐标、outputdebug 源偏移连接与行号换算。
 */
#include <catch2/catch_all.hpp>

#include <algorithm>
#include <set>

#include <fmt/format.h>

#include "decompile/Decompiler.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "runtime/Context.hpp"
#include "stdlib/StandardLibrary.hpp"
#include "test_config.h"
#include "tjsbc/TjsBytecodeReader.hpp"
#include "tjsbc/TjsTranslator.hpp"
#include "types/Class.hpp"
#include "types/Function.hpp"
#include "types/String.hpp"
#include "vm/VM.hpp"

using namespace cial;

namespace {
    Opt<vm::Chunk> compileSource(Runtime &runtime, Context &context, const std::string &text) {
        Common::Result diagnostics;
        Common::SourceFile source;
        source.load(diagnostics, text);
        if(diagnostics.isFailed())
            return std::nullopt;
        syntax::Parser parser{ source, context.pp() };
        auto *node = parser.parse(diagnostics);
        if(diagnostics.isFailed() || !node)
            return std::nullopt;
        // 输入契约 = 后优化 TAC（方案 §2）：走与常规编译一致的优化管线
        inter::IRGenerator generator{ diagnostics, runtime, source, [](vm::Chunk &chunk, OptReg result) {
                                         inter::optimizeDefaultChunk(chunk, result);
                                     } };
        OptReg result;
        return generator.parseAst(node, result);
    }

    const decompile::DecompiledUnit *findUnit(const decompile::DecompileResult &result, const std::string &name) {
        const auto it =
            std::find_if(result.units.begin(), result.units.end(), [&](const auto &unit) { return unit.name == name; });
        return it == result.units.end() ? nullptr : &*it;
    }
} // namespace

TEST_CASE("decompile - self path emits text and line table with half-open TAC coverage") {
    Runtime runtime;
    Context context{ runtime };
    auto chunk = compileSource(runtime, context, "function add(a, b) { return a + b; }\nvar r = add(2, 3);\n");
    REQUIRE(chunk.has_value());

    decompile::Options options{ .diagnosticView = true };
    const auto result = decompile::decompileChunk(runtime, *chunk, options);

    // M0：表达式/语句恢复未实现，严格状态如实 Unsupported 且逐单元给出原因
    REQUIRE(result.status == decompile::Status::Unsupported);
    REQUIRE_FALSE(result.diagnostics.empty());

    const auto *add = findUnit(result, "add");
    REQUIRE(add != nullptr);
    // M4：函数体经壳组装内联为 body 子单元（各自 Chunk 的 TAC 坐标）
    REQUIRE(add->kind == "body");

    // 行表不变量：非 synthetic 行的 TAC 区间按行序单调、逐条 [i, i+1)、
    // 并集完备覆盖所在 Chunk 的 [0, 指令数)。顶层单元对应顶层 Chunk；
    // M4 起函数体为 body 子单元，行表按语句区间登记（体首被内联的 TAC
    // 不产独立行，故只要求单调不重叠）
    const auto *topUnit = findUnit(result, "<top-level>");
    REQUIRE(topUnit != nullptr);
    {
        auto &instructions = chunk->getInstVec();
        size_t covered = 0;
        u32 expectedTac = 0;
        for(const auto &row : topUnit->lines) {
            if(row.synthetic) {
                REQUIRE(row.tacStart == row.tacEnd);
            } else {
                REQUIRE(row.tacStart == expectedTac);
                REQUIRE(row.tacEnd == row.tacStart + 1);
                expectedTac = row.tacEnd;
                ++covered;
            }
        }
        REQUIRE(covered == instructions.size());
    }
    {
        size_t covered = 0;
        u32 lastEnd = 0;
        for(const auto &row : add->lines) {
            if(row.synthetic) {
                REQUIRE(row.tacStart == row.tacEnd);
                continue;
            }
            REQUIRE(row.tacStart >= lastEnd);
            REQUIRE(row.tacEnd > row.tacStart);
            lastEnd = row.tacEnd;
            ++covered;
        }
        REQUIRE(covered > 0);
    }

    // 文本与行表一致：M4 起 body 子单元文本内联父单元（自身 text 为空），
    // 行表描述最终文本位置；顶层单元行数仍与文本行数一致
    REQUIRE(add->text.empty());
    REQUIRE_FALSE(add->lines.empty());
    {
        size_t lineCount = 0;
        for(const char c : topUnit->text)
            lineCount += c == '\n';
        REQUIRE(lineCount == topUnit->lines.size());
    }
    // textLine 为跨单元全局游标：单元内单调递增、逐行 +1
    u32 expectedLine = add->lines.front().textLine;
    for(const auto &row : add->lines)
        REQUIRE(row.textLine == expectedLine++);
    // 全部单元的 textLine 覆盖输出文本无缺口（声明块行同时由父单元 synthetic
    // 拆行与子单元行共同登记，允许同一文本行多条行表记录）
    std::set<u32> textLines;
    for(const auto &unit : result.units)
        for(const auto &row : unit.lines)
            textLines.insert(row.textLine);
    REQUIRE_FALSE(textLines.empty());
    REQUIRE(*textLines.begin() == 0);
    REQUIRE(*textLines.rbegin() == textLines.size() - 1);

    // 自产路径无码字/源偏移坐标
    for(const auto &row : add->lines) {
        REQUIRE_FALSE(row.tjsWordStart.has_value());
        REQUIRE_FALSE(row.sourceOffset.has_value());
    }
}

TEST_CASE("decompile - TJS2 program view carries origin word coordinates") {
    Runtime runtime;
    Context context{ runtime };
    const auto file = tjsbc::readTjsBytecodeFile(std::string{ TEST_FILES_PATH } + "/tjsbc/test_function.tjsbc");
    tjsbc::TjsTranslator translator{ runtime, file };
    const auto program = translator.translate();
    REQUIRE(program.topLevel >= 0);

    // M1 来源表：tacIdx 升序、码字非降；无 debug 产物不伪造源偏移
    const auto &top = program.contexts[static_cast<size_t>(program.topLevel)];
    REQUIRE(top.chunk != nullptr);
    REQUIRE_FALSE(top.origin.empty());
    u32 lastTac = 0, lastWord = 0;
    bool first = true;
    for(const auto &origin : top.origin) {
        if(!first) {
            REQUIRE(origin.tacIdx >= lastTac);
            REQUIRE(origin.tjsWord >= lastWord);
        }
        first = false;
        lastTac = origin.tacIdx;
        lastWord = origin.tjsWord;
        REQUIRE_FALSE(origin.sourceOffset.has_value());
    }
    REQUIRE(lastTac < top.chunk->getInstVec().size());

    decompile::Options options{ .diagnosticView = true };
    const auto result = decompile::decompileProgram(runtime, program, options);
    REQUIRE(result.status == decompile::Status::Unsupported);
    // fixture 顶层 context 名为 "global"（STRING 池下标），按名字定位
    const auto *topUnit = findUnit(result, "global");
    if(topUnit == nullptr)
        topUnit = findUnit(result, "<top-level>");
    REQUIRE(topUnit != nullptr);
    bool anyWord = false;
    for(const auto &row : topUnit->lines)
        anyWord = anyWord || row.tjsWordStart.has_value();
    REQUIRE(anyWord);
}

TEST_CASE("decompile - origin joins debug source positions and converts to lines") {
    Runtime runtime;
    Context context{ runtime };

    // 手工函数 context（f(x) = x*2，tjsbc_translator_test 同款码字），附带
    // outputdebug 产物才有的 codePos/sourcePos 平行数组
    tjsbc::TjsBytecodeFile file{};
    file.topLevel = -1;
    tjsbc::TjsContext ctx{};
    ctx.parent = -1;
    ctx.type = tjsbc::TjsContextType::Function;
    ctx.maxVariableCount = 1;
    ctx.variableReserveCount = 2;
    ctx.maxFrameCount = 2;
    ctx.funcDeclArgCount = 1;
    ctx.propSetter = ctx.propGetter = ctx.superClassGetter = -1;
    ctx.code = { 2, 1, -3, 1, 2, 0, 78, 1, 2, 118, 1, 119 };
    ctx.data = { tjsbc::TjsDataSlot{ tjsbc::TJS_TYPE_BYTE, 0 } };
    ctx.codePos = { 0, 3, 6, 9, 11 };
    ctx.sourcePos = { 0, 20, 21, 23, 27 };
    file.pool.bytes.push_back(2);
    file.contexts.push_back(ctx);

    tjsbc::TjsTranslator translator{ runtime, file };
    const auto program = translator.translate();
    const auto &function = program.contexts[0];
    REQUIRE(function.chunk != nullptr);
    REQUIRE_FALSE(function.origin.empty());
    // 转译器在首条 TJS2 指令前发射 prologue（this/this/load/…），首条映射的
    // tacIdx = prologue 长度；全部来源必须落在指令范围内且 tacIdx 升序
    REQUIRE(function.origin.front().tjsWord == 0);
    REQUIRE(function.origin.front().sourceOffset == 0);
    u32 lastTac = 0;
    bool first = true;
    for(const auto &origin : function.origin) {
        if(!first)
            REQUIRE(origin.tacIdx >= lastTac);
        first = false;
        lastTac = origin.tacIdx;
        REQUIRE(lastTac < function.chunk->getInstVec().size());
    }
    // ≤tjsWord 的最后一项连接：w3 → src20、w9 → src23
    bool saw20 = false, saw23 = false;
    for(const auto &origin : function.origin) {
        REQUIRE(origin.sourceOffset.has_value());
        if(origin.tjsWord == 3)
            saw20 = *origin.sourceOffset == 20;
        if(origin.tjsWord == 9)
            saw23 = *origin.sourceOffset == 23;
    }
    REQUIRE(saw20);
    REQUIRE(saw23);

    // 提供原源码时行表换算出行号；偏移按 UTF-16 码元，本例 ASCII 直接可用
    Common::Result diagnostics;
    Common::SourceFile source;
    source.load(diagnostics, "function f(x) {\n  return x * 2;\n}\n");
    REQUIRE_FALSE(diagnostics.isFailed());
    decompile::Options options{ .diagnosticView = true, .originalSource = &source };
    const auto result = decompile::decompileProgram(runtime, program, options);
    const auto *unit = findUnit(result, "<anonymous>");
    REQUIRE(unit != nullptr);
    bool sawLine0 = false, sawLine1 = false;
    for(const auto &row : unit->lines)
        if(row.sourceOffset) {
            REQUIRE(row.sourceLine.has_value());
            sawLine0 = sawLine0 || *row.sourceLine == 0;
            sawLine1 = sawLine1 || *row.sourceLine == 1;
        }
    REQUIRE(sawLine0);
    REQUIRE(sawLine1);
}

// ── M2：表达式恢复（ExprBuilder）──

#include "decompile/ExprBuilder.hpp"

namespace {
    // 顶层 Chunk 常量图中按名查找函数 FuncMeta
    FuncMeta *findFunction(Runtime &runtime, vm::Chunk &top, const std::string &name) {
        for(auto &constant : top.getConstants()) {
            if(constant.type() != ConstantType::FuncMeta)
                continue;
            auto *meta = constant.value<FuncMeta *>();
            if(const auto *entry = runtime.atomTable.get(meta->name);
               entry && entry->str && entry->str->toStdStr() == name)
                return meta;
        }
        return nullptr;
    }

    Vec<std::string> statementTexts(const decompile::ExprRecovery &recovery) {
        Vec<std::string> out;
        for(const auto &statement : recovery.statements)
            out.push_back(statement.text);
        return out;
    }

    // 无捕获原生捕获函数（thin 路径，与 stdlib 注册形态一致）；结果经命名空间
    // 级指针交付，用后即断
    Vec<std::string> *g_rtCapture = nullptr;

    std::string formatCapturedValue(const Value &val) {
        if(val.isVoid())
            return "void";
        if(val.isInteger())
            return fmt::format("{}", val.asInteger().unwrap());
        if(val.isReal())
            return fmt::format("{}", val.asReal().unwrap().value());
        if(val.isNull())
            return "null";
        if(val.isString())
            return fmt::format("\"{}\"", val.asString().unwrap()->toStdStr());
        return "other";
    }

    NativeFunction g_rtCaptureFn{ [](VM *, const Value &, size_t, Value val) {
        if(g_rtCapture != nullptr)
            g_rtCapture->push_back(formatCapturedValue(val));
    } };
} // namespace

TEST_CASE("decompile M2 - function body recovers locals, params, fused immediates and return") {
    Runtime runtime;
    Context context{ runtime };
    auto chunk = compileSource(runtime, context,
                               "function f(x, y) {\n"
                               "  var t = x * 2 + 1;\n"
                               "  t = t + y;\n"
                               "  return t > 3 && x < 10;\n"
                               "}\n");
    REQUIRE(chunk.has_value());
    auto *meta = findFunction(runtime, *chunk, "f");
    REQUIRE(meta != nullptr);

    const auto recovery = decompile::recoverUnitBody(runtime, *meta->chunk, meta, true);
    INFO(recovery.unsupported);
    REQUIRE(recovery.unsupported.empty());
    // BinaryImm 逆映射（x * 2、+ 1）、具名局部 var 声明、重赋值不带 var、
    // LAnd 非短路双求值、显式 return；末尾隐式 void return 省略。
    // M4 复合赋值逆融合：t = t + y 单读者模板合并为 t += y
    REQUIRE(statementTexts(recovery) == Vec<std::string>{ "var t = x * 2 + 1;", "t += y;", "return t > 3 && x < 10;" });
}

TEST_CASE("decompile M2 - multi-use temporaries materialize in function bodies") {
    Runtime runtime;
    Context context{ runtime };
    // 链式赋值共享一个调用结果寄存器：函数体内物化为临时变量，调用只发生一次
    auto chunk = compileSource(runtime, context,
                               "function g(o) {\n"
                               "  var a, b;\n"
                               "  a = b = o.next();\n"
                               "  return a;\n"
                               "}\n");
    REQUIRE(chunk.has_value());
    auto *meta = findFunction(runtime, *chunk, "g");
    REQUIRE(meta != nullptr);

    const auto recovery = decompile::recoverUnitBody(runtime, *meta->chunk, meta, true);
    INFO(recovery.unsupported);
    REQUIRE(recovery.unsupported.empty());
    const auto texts = statementTexts(recovery);
    // 恰一次 o.next() 调用，且先物化再两次读取临时名
    size_t calls = 0;
    for(const auto &text : texts)
        calls += text.find("o.next()") != std::string::npos;
    REQUIRE(calls == 1);
    const auto materialized = std::find_if(texts.begin(), texts.end(), [](const std::string &text) {
        return text.rfind("var v", 0) == 0 && text.find("o.next()") != std::string::npos;
    });
    REQUIRE(materialized != texts.end());
}

TEST_CASE("decompile M2 - strict self path succeeds on straight-line top level with full line coverage") {
    Runtime runtime;
    Context context{ runtime };
    auto chunk = compileSource(runtime, context,
                               "var a = [1, \"s\", 2.5];\n"
                               "var d = %[\"k\" => a[0], \"z\" => 2];\n"
                               "d.k = d.k + 4;\n"
                               "println(a[1], d.k, -a[0], typeof d);\n");
    REQUIRE(chunk.has_value());

    decompile::Options options{ .diagnosticView = false };
    const auto result = decompile::decompileChunk(runtime, *chunk, options);
    INFO((result.diagnostics.empty() ? std::string{} : result.diagnostics.front()));
    REQUIRE(result.status == decompile::Status::Success);
    REQUIRE(result.diagnostics.empty());

    const auto *top = findUnit(result, "<top-level>");
    REQUIRE(top != nullptr);
    REQUIRE(top->text.find("var a = [1, \"s\", 2.5];") != std::string::npos);
    REQUIRE(top->text.find("var d = %[\"k\" => a[0], \"z\" => 2];") != std::string::npos);
    // M4 复合赋值逆融合：d.k = d.k + 4 单读者模板合并为 d.k += 4
    REQUIRE(top->text.find("d.k += 4;") != std::string::npos);
    REQUIRE(top->text.find("println(a[1], d.k, -a[0], typeof d);") != std::string::npos);
    // 严格模式行表：非 synthetic 行按序闭合，末行延到 Chunk 末尾（覆盖被内联的 TAC）
    u32 expected = 0;
    u32 last = 0;
    for(const auto &row : top->lines) {
        if(row.synthetic)
            continue;
        REQUIRE(row.tacStart == expected);
        REQUIRE(row.tacEnd > row.tacStart);
        expected = row.tacEnd;
        last = row.tacEnd;
    }
    REQUIRE(last == chunk->getInstVec().size());
}

TEST_CASE("decompile M2 - structure leftovers are rejected with batch-specific reasons") {
    Runtime runtime;
    Context context{ runtime };
    struct Case {
        const char *source;
        const char *reason;
    };
    const Case cases[] = {
        // M5 首批已恢复封闭 switch；异常表仍按批次拒绝
        { "try { throw 1; } catch (e) { var a = e; }", "M5" },
        { "var o = %[];\nwith (o) { println(1); }\n", "M3" },
    };
    for(const auto &[source, reason] : cases) {
        auto chunk = compileSource(runtime, context, source);
        REQUIRE(chunk.has_value());
        decompile::Options options{ .diagnosticView = false };
        const auto result = decompile::decompileChunk(runtime, *chunk, options);
        INFO(source);
        REQUIRE(result.status == decompile::Status::Unsupported);
        REQUIRE_FALSE(result.diagnostics.empty());
        REQUIRE(result.diagnostics.front().find(reason) != std::string::npos);
    }
}

TEST_CASE("decompile M4 - declarations assemble into recompilable shells") {
    Runtime runtime;
    Context context{ runtime };
    // 函数声明（顶层安装链）+ 类（构造器/字段初始化/方法）+ property（getter/setter）
    auto chunk = compileSource(runtime, context,
                               "function add(a, b) { return a + b; }\n"
                               "class Box {\n"
                               "    var v = 10;\n"
                               "    function Box() { v += 5; }\n"
                               "    function get() { return v; }\n"
                               "}\n"
                               "var value;\n"
                               "property p {\n"
                               "    setter(x) { value = x; }\n"
                               "    getter { return value; }\n"
                               "}\n"
                               "println(add(1, 2));\n");
    REQUIRE(chunk.has_value());
    decompile::Options options{ .diagnosticView = false };
    const auto result = decompile::decompileChunk(runtime, *chunk, options);
    INFO((result.diagnostics.empty() ? std::string{} : result.diagnostics.front()));
    REQUIRE(result.status == decompile::Status::Success);
    const auto *top = findUnit(result, "<top-level>");
    REQUIRE(top != nullptr);
    const auto &text = top->text;
    REQUIRE(text.find("function add(a, b) {") != std::string::npos);
    REQUIRE(text.find("class Box {") != std::string::npos);
    REQUIRE(text.find("function Box() {") != std::string::npos);
    REQUIRE(text.find("var v = 10;") != std::string::npos);
    REQUIRE(text.find("property p {") != std::string::npos);
    REQUIRE(text.find("getter {") != std::string::npos);
    REQUIRE(text.find("setter(x) {") != std::string::npos);
    REQUIRE(text.find("println(add(1, 2));") != std::string::npos);
}

TEST_CASE("decompile M2 - diagnostic view annotates recovered statements without claiming success") {
    Runtime runtime;
    Context context{ runtime };
    auto chunk = compileSource(runtime, context, "var a = 1 + 2;\nprintln(a);\n");
    REQUIRE(chunk.has_value());
    decompile::Options options{ .diagnosticView = true };
    const auto result = decompile::decompileChunk(runtime, *chunk, options);
    // 视图模式恒不含可重编译承诺；恢复结果以注释附加
    REQUIRE(result.status == decompile::Status::Unsupported);
    const auto *top = findUnit(result, "<top-level>");
    REQUIRE(top != nullptr);
    REQUIRE(top->text.find("//   >> println(a);") != std::string::npos);
}

// ── M3：语句结构化（if/else、while、do-while、break/continue、三元折叠）──

namespace {
    // 编译整程序，取函数 f 的 FuncMeta，对输入矩阵执行并收集返回值
    // 与 CLI 同路径执行：vm.eval（内部走 Compilation 编译 + execute）。结果经
    // 注册的原生捕获函数收集（不经 stdout，避免与 Catch2 输出互相污染）
    Vec<std::string> runProgramOutputs(const std::string &program, const Vec<Integer> &inputs) {
        // 每次独立环境：registerStandardLibrary 不可对同一 Context 重复调用
        Runtime runtime;
        Context context{ runtime };
        stdlib::registerStandardLibrary(context);
        Vec<std::string> captured;
        g_rtCapture = &captured;
        context.registryGlobalFunc("__rt_capture"_str, &g_rtCaptureFn);
        vm::VMState state{ context };
        VM vm{ &state };

        std::string calls;
        for(size_t i = 0; i < inputs.size(); ++i)
            calls += fmt::format("__rt_capture(f({}));\n", inputs[i]);
        // 与 CLI execute 同语义：顶层帧 this = global（eval 路径缺这一步，
        // 函数声明绑定的 objthis 为空会改变名称解析）
        auto chunk = compileSource(runtime, context, program + "\n" + calls);
        REQUIRE(chunk.has_value());
        auto guarded = std::move(*chunk).toHeap(runtime);
        state.allocCallFrame(guarded.get());
        state.makeClosure();
        state.curFrame()->setThisObj(Value{ static_cast<cial::Object *>(context.global()) });
        state.resetFault();
        try {
            state.run();
        } catch(const std::exception &e) {
            const auto &fault = state.fault();
            std::string frames;
            for(const auto &frame : fault.frames)
                frames += fmt::format(" [{} @{}]", frame.function, frame.hasLocation ? frame.line : -1);
            const std::string detail = fmt::format(
                "run threw: {} fault {} {}:{}{}\n=== PROGRAM ===\n{}\n=== TAC ===\n{}", e.what(), fault.valid,
                fault.source, fault.line, frames, program, guarded->dumpInstructions().toStdStr());
            FAIL(detail);
        }
        state.freeCallFrame();
        REQUIRE(captured.size() == inputs.size()); // 一调用一结果
        g_rtCapture = nullptr;
        return captured;
    }

    // 恢复函数体 → 重包 function 壳为完整程序
    std::string rebuildFunctionProgram(Runtime &runtime, vm::Chunk &top, const std::string &name,
                                       const std::string &params) {
        auto *meta = findFunction(runtime, top, name);
        REQUIRE(meta != nullptr);
        const auto recovery = decompile::recoverUnitBody(runtime, *meta->chunk, meta, true);
        INFO(recovery.unsupported);
        REQUIRE(recovery.unsupported.empty());
        std::string text = fmt::format("function {}({}) {{\n", name, params);
        for(const auto &statement : recovery.statements)
            text += statement.text + "\n";
        text += "}\n";
        return text;
    }
} // namespace

TEST_CASE("decompile M3 - loop bodies round-trip behavior through recovery") {
    Runtime runtimeA;
    Context contextA{ runtimeA };
    const Vec<Integer> inputs = { -1, 0, 1, 2, 3, 4, 5, 9 };

    struct Case {
        const char *body;
        const char *params;
    };
    const Case cases[] = {
        // while + continue（continue 跳过自增 → 迭代次数对拍）
        { "var i = 0;\nwhile (i < x) {\ni = i + 1;\nif (i == 3) {\ncontinue;\n}\ni = i + 2;\n}\nreturn i;", "x" },
        // do-while + break
        { "do {\nx = x - 1;\nif (x == 2) {\nbreak;\n}\n} while (x > 0);\nreturn x;", "x" },
        // for（while 形态恢复）+ 三元折叠
        { "var s = 0;\nfor (var i = 0; i < x; i = i + 1) {\nif (i == 2) {\ncontinue;\n}\ns = s + i;\n}\n"
          "return s > 10 ? 1 : 0;",
          "x" },
        // else-if 链（嵌套区域）
        { "if (x > 5) {\nreturn 1;\n} else if (x > 2) {\nreturn 2;\n} else {\nreturn 3;\n}", "x" },
        // while(true) + break + 中途 return void
        { "while (true) {\nx = x + 1;\nif (x > 3) {\nbreak;\n}\n}\nif (x == 4) {\nreturn;\n}\nreturn x;", "x" },
    };

    for(const auto &[body, params] : cases) {
        const std::string original = fmt::format("function f({}) {{\n{}\n}}\n", params, body);
        // 预防：原程序须可编译
        {
            auto chunk = compileSource(runtimeA, contextA, original);
            REQUIRE(chunk.has_value());
        }
        DYNAMIC_SECTION(body) {
            auto chunk = compileSource(runtimeA, contextA, original);
            REQUIRE(chunk.has_value());
            const auto rebuilt = rebuildFunctionProgram(runtimeA, *chunk, "f", params);
            const auto expected = runProgramOutputs(original, inputs);
            const auto actual = runProgramOutputs(rebuilt, inputs);
            INFO(rebuilt);
            REQUIRE(actual == expected);
        }
    }
}

TEST_CASE("decompile M3 - top-level for with register-carried init variable is rejected") {
    Runtime runtime;
    Context context{ runtime };
    // 顶层 for 的初始化变量以寄存器携带（无名字绑定），条件按初值渲染会丢迭代
    auto chunk = compileSource(runtime, context, "var s = 0;\nfor (var i = 0; i < 3; i = i + 1) {\ns = s + i;\n}\n");
    REQUIRE(chunk.has_value());
    decompile::Options options{ .diagnosticView = false };
    const auto result = decompile::decompileChunk(runtime, *chunk, options);
    REQUIRE(result.status == decompile::Status::Unsupported);
    REQUIRE_FALSE(result.diagnostics.empty());
    REQUIRE(result.diagnostics.front().find("循环携带变量") != std::string::npos);
}

TEST_CASE("decompile M5 - direct flag comparisons preserve condition semantics") {
    const char *operators[] = { "==", "===", "<", ">" };
    for(size_t kind = 0; kind < 4; ++kind) {
        Runtime runtime;
        Context context{ runtime };
        const auto source =
            fmt::format("function f(x) {{ if (x {} -x) {{ return 1; }} else {{ return 2; }} }}", operators[kind]);
        auto top = compileSource(runtime, context, source);
        REQUIRE(top.has_value());
        auto *meta = findFunction(runtime, *top, "f");
        REQUIRE(meta != nullptr);
        auto &code = meta->chunk->getInstVec();
        size_t cmpPc = code.size();
        for(size_t pc = 0; pc < code.size(); ++pc) {
            if(code[pc]->opcode() != inter::TacOpCode::CompareTest)
                continue;
            const auto lhs = code[pc]->getOp1().value<inter::Register>();
            const auto rhs = code[pc]->getOp2().value<inter::Register>();
            if(kind == 0)
                meta->chunk->repl<inter::CmpEQ>(pc, lhs, rhs);
            if(kind == 1)
                meta->chunk->repl<inter::CmpDEQ>(pc, lhs, rhs);
            if(kind == 2)
                meta->chunk->repl<inter::CmpLT>(pc, lhs, rhs);
            if(kind == 3)
                meta->chunk->repl<inter::CmpGT>(pc, lhs, rhs);
            cmpPc = pc;
            break;
        }
        REQUIRE(cmpPc < code.size());
        const auto rebuilt = rebuildFunctionProgram(runtime, *top, "f", "x");
        INFO(rebuilt);
        REQUIRE(runProgramOutputs(rebuilt, { -2, 0, 2 }) == runProgramOutputs(source, { -2, 0, 2 }));
        REQUIRE(code[cmpPc + 1]->opcode() == inter::TacOpCode::JmpNE);
        const auto target = code[cmpPc + 1]->getOp1().value<inter::Label>();
        meta->chunk->repl<inter::JmpE>(cmpPc + 1, target);
        const auto inverse = rebuildFunctionProgram(runtime, *top, "f", "x");
        const auto expectedInverse =
            fmt::format("function f(x) {{ if (!(x {} -x)) {{ return 1; }} else {{ return 2; }} }}", operators[kind]);
        REQUIRE(runProgramOutputs(inverse, { -2, 0, 2 }) == runProgramOutputs(expectedInverse, { -2, 0, 2 }));
    }
}

namespace {
    void makeIrreducibleChunk(vm::Chunk &chunk, bool truthy = false) {
        using namespace cial::inter;
        chunk.getInstVec().clear();
        chunk.setRegCount(4);
        chunk.emit<LoadImm>(Register{ 1 }, Integer{ 0 });
        chunk.emit<LoadImm>(Register{ 2 }, Integer{ 0 });
        if(truthy)
            chunk.emit<Test>(Register{ 0 });
        else
            chunk.emitRaw<CompareTestImm>(TacOpCode::GT, Register{ 0 }, Integer{ 0 }, Register{ 3 });
        chunk.emit<JmpNE>(Label{ 8 });
        chunk.emitRaw<BinaryImm>(TacOpCode::Add, Register{ 2 }, Integer{ 1 }, Register{ 2 });
        chunk.emitRaw<BinaryImm>(TacOpCode::Add, Register{ 1 }, Integer{ 1 }, Register{ 1 });
        chunk.emitRaw<CompareTestImm>(TacOpCode::LT, Register{ 1 }, Integer{ 3 }, Register{ 3 });
        chunk.emit<JmpNE>(Label{ 14 });
        chunk.emitRaw<BinaryImm>(TacOpCode::Add, Register{ 2 }, Integer{ 10 }, Register{ 2 });
        chunk.emitRaw<BinaryImm>(TacOpCode::Add, Register{ 1 }, Integer{ 1 }, Register{ 1 });
        chunk.emitRaw<CompareTestImm>(TacOpCode::LT, Register{ 1 }, Integer{ 3 }, Register{ 3 });
        chunk.emit<JmpNE>(Label{ 14 });
        chunk.emit<Jmp>(Label{ 4 });
        chunk.emit<NOP>();
        chunk.emit<inter::Ret>(Register{ 2 });
    }
} // namespace

TEST_CASE("decompile M5 - irreducible scalar CFG falls back without losing carried values") {
    using namespace cial::inter;
    Runtime runtime;
    Context context{ runtime };
    const std::string parameter = GENERATE(std::string{ "x" }, std::string{ "v0" });
    const bool observable = GENERATE(false, true);
    auto top = compileSource(runtime, context, fmt::format("function f({0}) {{ return {0}; }}", parameter));
    REQUIRE(top.has_value());
    auto *meta = findFunction(runtime, *top, "f");
    REQUIRE(meta != nullptr);
    auto rooted = std::move(*top).toHeap(runtime);
    auto &chunk = *meta->chunk;
    makeIrreducibleChunk(chunk);
    const auto effect = runtime.atomTable.intern("v1"_str);
    if(observable) {
        chunk.repl<DGlobal>(4, effect, Register{ 1 });
        context.global()->setProp(effect, Value{ Integer{ 0 } });
    }
    const auto rebuilt = rebuildFunctionProgram(runtime, *rooted.get(), "f", parameter);
    INFO(rebuilt);
    REQUIRE(rebuilt.find("while (true)") != std::string::npos);
    REQUIRE(rebuilt.find("switch (") != std::string::npos);
    // Two entries into the SCC: positive inputs enter A, all others enter B.
    const Vec<Integer> inputs{ -3, 0, 1, 8 };
    REQUIRE(rebuilt.find(parameter + " = void;") != std::string::npos);
    if(observable) {
        REQUIRE(rebuilt.find("var v1 ") == std::string::npos);
        auto wrapped = rebuilt;
        wrapped.replace(wrapped.find("function f("), std::string{ "function f(" }.size(), "function body(");
        wrapped = "var v1=0;\n" + wrapped + "function f(x){var r=body(x);return r*100+v1;}\n";
        REQUIRE(runProgramOutputs(wrapped, inputs) ==
                runProgramOutputs("function f(x){return x>0?1002:2001;}", inputs));
    } else {
        REQUIRE(runProgramOutputs(rebuilt, inputs) == runProgramOutputs("function f(x){return x>0?12:21;}", inputs));
    }
    const auto mapped = decompile::decompileChunk(runtime, *rooted.get(), decompile::Options{});
    INFO((mapped.diagnostics.empty() ? std::string{} : mapped.diagnostics.front()));
    REQUIRE(mapped.status == decompile::Status::Success);
    const auto *unit = findUnit(mapped, "f");
    REQUIRE(unit != nullptr);
    REQUIRE(std::ranges::any_of(unit->lines, [](const auto &row) { return row.synthetic; }));
    for(const auto &row : unit->lines) {
        REQUIRE(row.tacEnd <= chunk.getInstVec().size());
        if(row.synthetic)
            REQUIRE(row.tacStart == row.tacEnd);
    }
    const auto topRejected = decompile::recoverUnitBody(runtime, chunk, meta, false);
    REQUIRE_FALSE(topRejected.unsupported.empty());
    REQUIRE(topRejected.statements.empty());
    vm::VMState state{ context };
    VM vm{ &state };
    const auto function = runtime.create<Function>(meta);
    for(const auto input : inputs) {
        const Value arg{ input };
        const auto value = vm.callValue(Value{ static_cast<Object *>(function.get()) }, 1, &arg);
        REQUIRE(value.isInteger());
        REQUIRE(value.asInteger().unwrap() == (observable ? (input > 0 ? 10 : 20) : (input > 0 ? 12 : 21)));
        if(observable)
            REQUIRE(context.global()->getProp(effect).asInteger().unwrap() == (input > 0 ? 2 : 1));
    }
    chunk.repl<GThis>(13, runtime.atomTable.intern(String{ parameter }), Register{ 3 });
    const auto collision = decompile::recoverUnitBody(runtime, chunk, meta, true);
    REQUIRE(collision.unsupported.find("参数与动态名字查找冲突") != std::string::npos);
    REQUIRE(collision.statements.empty());
    chunk.repl<Call>(13, Register{ 2 }, Register{ 3 }, Integer{ 0 });
    // 调用族白名单扩展后，状态机按 pc 重放调用 tile（不可达块同理）
    const auto call = decompile::recoverUnitBody(runtime, chunk, meta, true);
    INFO(call.unsupported);
    REQUIRE(call.unsupported.empty());
    REQUIRE_FALSE(call.statements.empty());
    REQUIRE(std::ranges::any_of(call.statements,
                                [](const auto &stmt) { return stmt.text.find("()") != std::string::npos; }));
    chunk.repl<EvalStr>(13, Register{ 0 }, Register{ 3 });
    const auto dynamic = decompile::recoverUnitBody(runtime, chunk, meta, true);
    REQUIRE(dynamic.unsupported.find("动态求值") != std::string::npos);
    REQUIRE(dynamic.statements.empty());
}

TEST_CASE("decompile M5 - state machine recovers method calls with inline receivers") {
    Runtime runtime;
    Context context{ runtime };
    // 具名局部 + try + 调用：结构化被声明落点守卫拒绝，落回状态机；
    // CallMethodV 的接收者经 memberCalleeInlineOk 内联渲染 o.m(...)
    const std::string source = "function f(x) {\n"
                               "    var r = 0;\n"
                               "    try { r = Math.abs(x); if (x > 1) throw r; } catch (e) { r = -1; }\n"
                               "    return r;\n"
                               "}\n";
    auto top = compileSource(runtime, context, source);
    REQUIRE(top.has_value());
    auto rooted = std::move(*top).toHeap(runtime);
    const auto rebuilt = rebuildFunctionProgram(runtime, *rooted.get(), "f", "x");
    INFO(rebuilt);
    REQUIRE(rebuilt.find("while (true)") != std::string::npos);
    REQUIRE(rebuilt.find(".abs(") != std::string::npos);
    REQUIRE(runProgramOutputs(rebuilt, { 0, 2 }) == runProgramOutputs(source, { 0, 2 }));
}

TEST_CASE("decompile M5 - CFG fallback releases object argument roots after return") {
    struct Tracked final : Object {
        int &destroyed;
        explicit Tracked(int &count) : destroyed(count) {}
        ~Tracked() override { ++destroyed; }
        void call(vm::VMState &, u32, size_t) override {}
    };
    int destroyedOriginal = 0;
    int destroyedRestored = 0;
    Runtime originalRuntime;
    Context originalContext{ originalRuntime };
    auto source = compileSource(originalRuntime, originalContext, "function f(x){return x;}");
    REQUIRE(source.has_value());
    auto originalRoot = std::move(*source).toHeap(originalRuntime);
    auto *original = findFunction(originalRuntime, *originalRoot.get(), "f");
    REQUIRE(original != nullptr);
    makeIrreducibleChunk(*original->chunk, true);
    const auto rebuilt = rebuildFunctionProgram(originalRuntime, *originalRoot.get(), "f", "x");
    Runtime restoredRuntime;
    Context restoredContext{ restoredRuntime };
    auto restoredSource = compileSource(restoredRuntime, restoredContext, rebuilt);
    REQUIRE(restoredSource.has_value());
    auto restoredRoot = std::move(*restoredSource).toHeap(restoredRuntime);
    auto *restored = findFunction(restoredRuntime, *restoredRoot.get(), "f");
    REQUIRE(restored != nullptr);
    for(const bool replay : { false, true }) {
        auto &runtime = replay ? restoredRuntime : originalRuntime;
        auto &context = replay ? restoredContext : originalContext;
        auto *meta = replay ? restored : original;
        int &destroyed = replay ? destroyedRestored : destroyedOriginal;
        vm::VMState state{ context };
        VM vm{ &state };
        const auto function = runtime.create<Function>(meta);
        Value argument;
        {
            const auto object = runtime.create<Tracked>(destroyed);
            argument = Value{ static_cast<Object *>(object.get()) };
        }
        const auto result = vm.callValue(Value{ static_cast<Object *>(function.get()) }, 1, &argument);
        REQUIRE(result.asInteger().unwrap() == 12);
        runtime.markSweep.collect([&] { runtime.collectAllMark(); });
        REQUIRE(destroyed == 1);
    }
}

TEST_CASE("decompile M5 - bounded generated CFGs agree with VM and alternating-sum oracle") {
    using namespace cial::inter;
    const Vec<Integer> inputs{ -4, 0, 1, 7 };
    for(Integer trial = 0; trial < 8; ++trial) {
        DYNAMIC_SECTION("graph " << trial) {
            Runtime runtime;
            Context context{ runtime };
            auto source = compileSource(runtime, context, "function f(x){return x;}");
            REQUIRE(source.has_value());
            auto rooted = std::move(*source).toHeap(runtime);
            auto *meta = findFunction(runtime, *rooted.get(), "f");
            REQUIRE(meta != nullptr);
            auto &chunk = *meta->chunk;
            makeIrreducibleChunk(chunk);
            const Integer a = trial % 3 - 2;
            const Integer b = trial + 3;
            const Integer limit = trial % 5 + 1;
            chunk.getInstVec()[4] = std::make_unique<BinaryImm>(TacOpCode::Add, Register{ 2 }, a, Register{ 2 });
            chunk.getInstVec()[8] = std::make_unique<BinaryImm>(TacOpCode::Add, Register{ 2 }, b, Register{ 2 });
            for(const size_t pc : { 6u, 10u })
                chunk.getInstVec()[pc] =
                    std::make_unique<CompareTestImm>(TacOpCode::LT, Register{ 1 }, limit, Register{ 3 });
            const auto rebuilt = rebuildFunctionProgram(runtime, *rooted.get(), "f", "x");
            REQUIRE(rebuilt.find("switch (") != std::string::npos);
            Vec<std::string> expected;
            vm::VMState state{ context };
            VM vm{ &state };
            const auto function = runtime.create<Function>(meta);
            for(const auto input : inputs) {
                Integer sum = 0;
                bool useA = input > 0;
                for(Integer i = 0; i < limit; ++i) {
                    sum += useA ? a : b;
                    useA = !useA;
                }
                const Value argument{ input };
                REQUIRE(
                    vm.callValue(Value{ static_cast<Object *>(function.get()) }, 1, &argument).asInteger().unwrap() ==
                    sum);
                expected.push_back(std::to_string(sum));
            }
            REQUIRE(runProgramOutputs(rebuilt, inputs) == expected);
        }
    }
}

TEST_CASE("decompile M5 - exception slot zero and absent slot preserve exact register state") {
    using namespace cial::inter;
    const bool keepException = GENERATE(false, true);
    Runtime runtime;
    Context context{ runtime };
    auto source = compileSource(runtime, context, "function f(x){return x;}");
    REQUIRE(source.has_value());
    auto rooted = std::move(*source).toHeap(runtime);
    auto *meta = findFunction(runtime, *rooted.get(), "f");
    REQUIRE(meta != nullptr);
    auto &chunk = *meta->chunk;
    chunk.getInstVec().clear();
    chunk.getThrowHandlers().clear();
    chunk.setRegCount(2);
    chunk.emit<LoadImm>(Register{ 1 }, Integer{ 7 });
    chunk.emit<Throw>(Register{ 1 });
    chunk.emit<LoadImm>(Register{ 1 }, Integer{ 100 });
    chunk.emit<Jmp>(Label{ 6 });
    chunk.emit<inter::Ret>(Register{ 0 });
    chunk.emit<NOP>();
    chunk.emit<inter::Ret>(Register{ 1 });
    chunk.addThrowHandler({ 0, 4, keepException ? u16{ 0 } : vm::ThrowHandler::exValueRegNone });
    const auto rebuilt = rebuildFunctionProgram(runtime, *rooted.get(), "f", "x");
    INFO(rebuilt);
    // 有槽 = catch (名字)；无槽 = 裸 catch（结构化路径；状态机输出恒带括号）
    REQUIRE((rebuilt.find("catch (") != std::string::npos || rebuilt.find("catch {") != std::string::npos));
    const Vec<Integer> inputs{ -2, 0, 2 };
    REQUIRE(runProgramOutputs(rebuilt, inputs) ==
            runProgramOutputs(keepException ? "function f(x){return 7;}" : "function f(x){return x;}", inputs));
    vm::VMState state{ context };
    VM vm{ &state };
    const auto function = runtime.create<Function>(meta);
    for(const auto input : inputs) {
        const Value argument{ input };
        REQUIRE(vm.callValue(Value{ static_cast<Object *>(function.get()) }, 1, &argument).asInteger().unwrap() ==
                (keepException ? 7 : input));
    }
    chunk.getThrowHandlers()[0].tryEnd = chunk.getInstVec().size();
    const auto invalid = decompile::recoverUnitBody(runtime, chunk, meta, true);
    REQUIRE(invalid.unsupported.find("catch 入口越界") != std::string::npos);
    REQUIRE(invalid.statements.empty());
}

TEST_CASE("decompile M5 - rethrow crosses function boundary with original exception value") {
    Runtime runtime;
    Context context{ runtime };
    const std::string original = "function g(x){try{throw x;}catch(e){if(e<0)throw e;return e+10;}}";
    const std::string caller = "function f(x){try{return g(x);}catch(e){return e-100;}}";
    auto top = compileSource(runtime, context, original);
    REQUIRE(top.has_value());
    auto rooted = std::move(*top).toHeap(runtime);
    const auto rebuilt = rebuildFunctionProgram(runtime, *rooted.get(), "g", "x");
    REQUIRE(runProgramOutputs(rebuilt + caller, { -4, -1, 0, 2 }) ==
            runProgramOutputs(original + caller, { -4, -1, 0, 2 }));
}

TEST_CASE("decompile M5 - caught opaque objects preserve identity") {
    struct Token final : Object {
        void call(vm::VMState &, u32, size_t) override {}
    };
    Runtime runtime;
    Context context{ runtime };
    auto source = compileSource(runtime, context, "function f(x){try{throw x;}catch(e){return e;}}");
    REQUIRE(source.has_value());
    auto originalRoot = std::move(*source).toHeap(runtime);
    auto *original = findFunction(runtime, *originalRoot.get(), "f");
    REQUIRE(original != nullptr);
    const auto text = rebuildFunctionProgram(runtime, *originalRoot.get(), "f", "x");
    auto restored = compileSource(runtime, context, text);
    REQUIRE(restored.has_value());
    auto restoredRoot = std::move(*restored).toHeap(runtime);
    auto *restoredMeta = findFunction(runtime, *restoredRoot.get(), "f");
    REQUIRE(restoredMeta != nullptr);
    const auto token = runtime.create<Token>();
    const Value argument{ static_cast<Object *>(token.get()) };
    vm::VMState state{ context };
    VM vm{ &state };
    for(auto *meta : { original, restoredMeta }) {
        const auto function = runtime.create<Function>(meta);
        const auto result = vm.callValue(Value{ static_cast<Object *>(function.get()) }, 1, &argument);
        REQUIRE(result.isObject());
        REQUIRE(result.asObject().unwrap() == token.get());
    }
}

TEST_CASE("decompile M5 - structured try/catch recovers region without state machine") {
    Runtime runtime;
    Context context{ runtime };
    // catch 体只读异常槽：结构化路径输出真 try/catch，不落状态机平铺
    const std::string source = "function f(x){try{if(x)throw x;}catch(e){return e;}return -1;}";
    auto top = compileSource(runtime, context, source);
    REQUIRE(top.has_value());
    auto rooted = std::move(*top).toHeap(runtime);
    const auto rebuilt = rebuildFunctionProgram(runtime, *rooted.get(), "f", "x");
    INFO(rebuilt);
    REQUIRE(rebuilt.find("try {") != std::string::npos);
    REQUIRE(rebuilt.find("catch (") != std::string::npos);
    REQUIRE(rebuilt.find("while (true)") == std::string::npos);
    REQUIRE(runProgramOutputs(rebuilt, { -2, 0, 5 }) == runProgramOutputs(source, { -2, 0, 5 }));
}

TEST_CASE("decompile M5 - nested structured try regions keep inner rethrow semantics") {
    Runtime runtime;
    Context context{ runtime };
    const std::string source = "function f(x){try{try{throw x;}catch(a){throw a+1;}}catch(b){return b;}return -1;}";
    auto top = compileSource(runtime, context, source);
    REQUIRE(top.has_value());
    auto rooted = std::move(*top).toHeap(runtime);
    const auto rebuilt = rebuildFunctionProgram(runtime, *rooted.get(), "f", "x");
    INFO(rebuilt);
    REQUIRE(rebuilt.find("try {") != std::string::npos);
    REQUIRE(rebuilt.find("while (true)") == std::string::npos);
    REQUIRE(runProgramOutputs(rebuilt, { -3, 0, 5 }) == runProgramOutputs(source, { -3, 0, 5 }));
}

TEST_CASE("decompile M5 - structured bare catch without exception slot") {
    Runtime runtime;
    Context context{ runtime };
    const std::string source = "function f(x){try{if(x)throw x;}catch{return 42;}return -1;}";
    auto top = compileSource(runtime, context, source);
    REQUIRE(top.has_value());
    auto rooted = std::move(*top).toHeap(runtime);
    const auto rebuilt = rebuildFunctionProgram(runtime, *rooted.get(), "f", "x");
    INFO(rebuilt);
    REQUIRE(rebuilt.find("} catch {") != std::string::npos);
    REQUIRE(rebuilt.find("while (true)") == std::string::npos);
    REQUIRE(runProgramOutputs(rebuilt, { 0, 9 }) == runProgramOutputs(source, { 0, 9 }));
}

TEST_CASE("decompile M5 - catch reading named try-body register falls back to state machine") {
    Runtime runtime;
    Context context{ runtime };
    // 具名局部不提升（优化器删除体外死初始化后幸存定义内移，`var` 声明落点
    // 随之漂移，块作用域下 catch 不可见）——数据流守卫拒绝，落回状态机且语义
    // 正确。无名寄存器的提升由固定 bc 语料端到端覆盖（manifest structured 标记）
    const std::string source = "function f(x){var r=0;try{r=x*3;if(x==0)throw 7;}catch(e){r=r+e;}return r;}";
    auto top = compileSource(runtime, context, source);
    REQUIRE(top.has_value());
    auto rooted = std::move(*top).toHeap(runtime);
    const auto rebuilt = rebuildFunctionProgram(runtime, *rooted.get(), "f", "x");
    INFO(rebuilt);
    REQUIRE(rebuilt.find("catch (") != std::string::npos);
    REQUIRE(rebuilt.find("while (true)") != std::string::npos);
    REQUIRE(runProgramOutputs(rebuilt, { 0, 5 }) == runProgramOutputs(source, { 0, 5 }));
}
