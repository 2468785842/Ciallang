/*
 * Copyright (c) 2026/11/1
 *
 * 反编译器 M0 测试（doc/bytecode/decompiler-plan.md）：
 * - 自产路径 {文本, 行表}：TAC 半开区间完备覆盖、synthetic 标记、严格状态；
 * - TJS2 路径来源表：TacOrigin 码字坐标、outputdebug 源偏移连接与行号换算。
 */
#include <catch2/catch_all.hpp>

#include <algorithm>

#include "decompile/Decompiler.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "runtime/Context.hpp"
#include "test_config.h"
#include "tjsbc/TjsBytecodeReader.hpp"
#include "tjsbc/TjsTranslator.hpp"
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
    REQUIRE(add->kind == "function");

    // 行表不变量：非 synthetic 行的 TAC 区间按行序单调、逐条 [i, i+1)、
    // 并集完备覆盖所在 Chunk 的 [0, 指令数)。顶层单元对应顶层 Chunk；
    // 函数单元的 Chunk 经常量图可达，其指令数由单元行数自证
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
        u32 expectedTac = 0;
        for(const auto &row : add->lines) {
            if(row.synthetic) {
                REQUIRE(row.tacStart == row.tacEnd);
            } else {
                REQUIRE(row.tacStart == expectedTac);
                REQUIRE(row.tacEnd == row.tacStart + 1);
                expectedTac = row.tacEnd;
                ++covered;
            }
        }
        REQUIRE(covered > 0);
        REQUIRE(expectedTac == covered); // 函数体连续无缺口
    }

    // 文本与行表一致：每行以 \n 结尾（含末行），行数 = '\n' 计数；
    // textLine 从 0 起单调递增
    size_t lineCount = 0;
    for(const char c : add->text)
        lineCount += c == '\n';
    REQUIRE(lineCount == add->lines.size());
    // textLine 为跨单元全局游标：单元内单调递增、逐行 +1
    u32 expectedLine = add->lines.front().textLine;
    for(const auto &row : add->lines)
        REQUIRE(row.textLine == expectedLine++);
    // 全部单元的 textLine 全局无重复且连续
    u32 globalLine = 0;
    for(const auto &unit : result.units)
        for(const auto &row : unit.lines)
            REQUIRE(row.textLine == globalLine++);

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
    // LAnd 非短路双求值、显式 return；末尾隐式 void return 省略
    REQUIRE(statementTexts(recovery) ==
            Vec<std::string>{ "var t = x * 2 + 1;", "t = t + y;", "return t > 3 && x < 10;" });
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
    REQUIRE(top->text.find("d.k = d.k + 4;") != std::string::npos);
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

TEST_CASE("decompile M2 - structure and declarations are rejected with batch-specific reasons") {
    Runtime runtime;
    Context context{ runtime };
    struct Case {
        const char *source;
        const char *reason;
    };
    const Case cases[] = {
        { "var a = 1;\nif (a) { println(a); }\n", "M3" },
        { "function f() { return 1; }\n", "M4" },
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
