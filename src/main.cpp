/*
 * Copyright (c) 2024/5/6 下午8:16
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

#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <fstream>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "gen/ControlFlowGraph.hpp"
#include "gen/SSA.hpp"
#include "stdlib/ExceptionLib.hpp"
#include "stdlib/Print.hpp"
#include "tjsbc/TjsBytecodeReader.hpp"
#include "tjsbc/TjsTranslator.hpp"
#include "types/Class.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

#if defined(_MSC_VER) && defined(_DEBUG)
// 把 Debug 断言从弹窗重定向到 stderr，终端/CI 下可见（Windows 断言弹窗在
// 无交互环境表现为"挂起"）；与 tests/fuzz_regress_test.cpp 同款
static const bool g_redirectCrtReport = [] {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    return true;
}();
#endif

static std::string jsonString(const std::string &text) {
    std::string out = "\"";
    for(const unsigned char c : text) {
        switch(c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if(c < 32)
                    out += fmt::format("\\u{:04x}", c);
                else
                    out += static_cast<char>(c);
        }
    }
    return out + '"';
}

int main(const int argc, char **argv) {
    if(argc < 2) {
        fmt::println("Usage: cial <script-file>\n       cial --dump-cfg <script-file>\n       cial --trace-execution "
                     "<script-file> <output.dot>\n       cial --jit|--trace-jit <script-file>\n       cial "
                     "--check-json <script-file>\n       cial --tail-recursion <script-file>\n       cial --dump-ssa "
                     "<script-file>");
        return 0;
    }

    // 无缓冲输出：崩溃时不丢失已执行的打印，便于定位
    setvbuf(stdout, nullptr, _IONBF, 0);

    const auto start = std::chrono::high_resolution_clock::now();
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    context.registryGlobalFunc("print"_str, &stdlib::S_PrintFunction);
    context.registryGlobalFunc("println"_str, &stdlib::S_PrintlnFunction);
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    // Debug.message 垫片:对拍语料(tests/test_files/tjs2)用 TJS2 的 Debug.message
    // 输出,Ciallang 以等价对象承接,保证同一脚本双引擎可原样运行
    vm.eval(R"(class __DebugShim {
    function __DebugShim() {}
    function message(s) { println(s); }
}
var Debug = new __DebugShim();
)"_str);
    // Exception 内建类(语料中 throw new Exception(...) 高频依赖)
    stdlib::initExceptionClass(vm);

    // LSP 等宿主使用：仅解析/编译，绝不执行被检查的脚本；stdout 是单个 JSON 值。
    if(std::string(argv[1]) == "--check-json") {
        Common::Result result;
        if(argc != 3) {
            result.error("Usage: cial --check-json <script-file>");
        } else {
            try {
                Common::SourceFile source{ argv[2] };
                source.load(result);
                if(!result.isFailed()) {
                    syntax::Parser parser{ source, context.pp() };
                    auto *node = parser.parse(result);
                    if(node && !result.isFailed()) {
                        inter::IRGenerator generator{ result, rt, source };
                        OptReg ignored;
                        (void)generator.parseAst(node, ignored);
                    } else if(!result.isFailed()) {
                        result.error("cannot parse script");
                    }
                }
            } catch(const std::exception &e) {
                result.error(e.what());
            }
        }
        fmt::print("{{\"ok\":{},\"columnEncoding\":\"utf-8\",\"diagnostics\":[", !result.isFailed());
        bool first = true;
        for(const auto &message : result.messages()) {
            if(!first)
                fmt::print(",");
            first = false;
            const auto &location = message.location();
            fmt::print("{{\"message\":{},\"severity\":{},\"line\":{},\"column\":{},\"endLine\":{},\"endColumn\":{}}}",
                       jsonString(message.message()), message.isError() ? 1 : 2, location.start().line,
                       location.start().column, location.end().line, location.end().column);
        }
        fmt::println("]}}");
        return result.isFailed() ? 1 : 0;
    }

    // --tjsbc <file.tjsbc>:加载 TJS2 字节码,转译进现有 VM 执行顶层 context
    // (doc/bytecode/mapping.md 转译路线;M1)
    if(std::string(argv[1]) == "--tjsbc") {
        if(argc < 3) {
            fmt::println("Usage: cial --tjsbc <bytecode-file>");
            return 1;
        }
        try {
            const auto file = tjsbc::readTjsBytecodeFile(argv[2]);
            tjsbc::TjsTranslator translator{ rt, file };
            const auto program = translator.translate();
            if(program.topLevel < 0)
                throw std::runtime_error("bytecode has no top-level context");
            auto *chunk = program.contexts[static_cast<size_t>(program.topLevel)].chunk;
            // CIAL_TJSBC_DUMP=1:打印顶层 context 转译产物 TAC；=all 打印全部 context（调试用）
            if(const char *dp = std::getenv("CIAL_TJSBC_DUMP"); dp && *dp) {
                if(std::string(dp) == "all") {
                    for(size_t i = 0; i < program.contexts.size(); ++i) {
                        if(program.contexts[i].chunk)
                            fmt::println("--- tjsbc ctx[{}] ---\n{}", i,
                                         program.contexts[i].chunk->dumpInstructions());
                    }
                } else {
                    fmt::println("--- tjsbc top-level TAC ---\n{}",
                                 chunk->dumpInstructions());
                }
            }
            vmState.allocCallFrame(chunk);
            vmState.makeClosure();
            // 顶层 this = 全局对象（TJS 顶层 SPDS/GPD 经 this/proxy 寄存器读写全局）
            vmState.curFrame()->setThisObj(
                Value{ static_cast<Object *>(vmState.context.global()) });
            try {
                if(const char *dp = std::getenv("CIAL_TJSBC_DUMP"); dp && *dp)
                    fmt::println(stderr, "[TJSBC] before run0");
                vmState.run0();
                if(const char *dp = std::getenv("CIAL_TJSBC_DUMP"); dp && *dp)
                    fmt::println(stderr, "[TJSBC] after run0");
            } catch(...) {
                vmState.clearException();
                throw;
            }
            vmState.freeCallFrame();
            fmt::println(stderr, "[TJSBC] done");
        } catch(const std::exception &e) {
            fmt::println(stderr, "cial: runtime error: {}", e.what());
            return 1;
        }
        const auto end = std::chrono::high_resolution_clock::now();
        const auto duration = duration_cast<std::chrono::nanoseconds>(end - start);
        fmt::println("Time taken by function: {}ns", duration.count());
        return 0;
    }

    // --dump-cfg <script.tjs>:编译(含优化)后输出各 chunk 控制流图 DOT,不执行脚本。
    // 节点=基本块(带 TAC 区间与指令文本),回边标红,循环头高亮,可用于核对
    // 优化器块划分与循环识别(ControlFlowGraph::getLoops)
    if(std::string(argv[1]) == "--dump-cfg" || std::string(argv[1]) == "--dump-ssa") {
        const bool dumpSsa = std::string(argv[1]) == "--dump-ssa";
        if(argc < 3) {
            fmt::println("Usage: cial --dump-cfg|--dump-ssa <script-file>");
            return 1;
        }
        try {
            Common::Result r{};
            Common::SourceFile sf{ argv[2] };
            sf.load(r);
            if(r.isFailed())
                throw std::runtime_error("cannot load source file");
            syntax::Parser parser{ sf, context.pp() };
            syntax::AstNode *node = parser.parse(r);
            if(!node || r.isFailed())
                throw std::runtime_error("cannot parse script");
            inter::IRGenerator codeGen{ r, rt, sf };
            OptReg ir{};
            auto topLevel = codeGen.parseAst(node, ir);
            if(!topLevel || r.isFailed())
                throw std::runtime_error("cannot compile script");
            vm::Chunk *topChunk = &topLevel.value();

            // 顶层 chunk + 常量池中 FuncMeta 指向的嵌套函数 chunk（递归、去重）
            auto funcName = [&](const FuncMeta *meta) {
                if(const auto *entry = rt.atomTable.get(meta->name); entry && entry->str && entry->str->length() > 0)
                    return entry->str->toStdStr();
                return std::string("func");
            };
            Vec<std::pair<std::string, vm::Chunk *>> pending{ { "top-level", topChunk } };
            Vec<vm::Chunk *> visited;
            while(!pending.empty()) {
                auto [name, chunk] = pending.back();
                pending.pop_back();
                if(std::ranges::find(visited, chunk) != visited.end())
                    continue;
                visited.push_back(chunk);
                for(const auto &constant : chunk->getConstants()) {
                    if(constant.type() != ConstantType::FuncMeta)
                        continue;
                    const auto *meta = constant.value<FuncMeta *>();
                    if(meta && meta->chunk)
                        pending.emplace_back(funcName(meta), meta->chunk);
                }
                inter::ControlFlowGraph cfg;
                cfg.buildCFG(*chunk);
                fmt::println("// ===== {} =====", name);
                if(dumpSsa) {
                    const auto form = inter::SsaBuilder::buildChunkSsa(*chunk, cfg.getBasicBlocks());
                    fmt::println("{}", inter::SsaBuilder::dumpSsaText(*chunk, form));
                } else {
                    fmt::println("{}", cfg.dumpDot(*chunk));
                }
            }
        } catch(const std::exception &e) {
            fmt::println(stderr, "cial: {}", e.what());
            return 1;
        }
        return 0;
    }

    if(std::string(argv[1]) == "--tail-recursion") {
        if(argc != 3) {
            fmt::println(stderr, "Usage: cial --tail-recursion <script-file>");
            return 1;
        }
        try {
            vmState.setTailRecursionEnabled(true);
            vm.eval(String(argv[2]), true);
            fmt::println(stderr, "[tail] eliminated={}", vmState.tailCallsEliminated());
        } catch(const std::exception &e) {
            fmt::println(stderr, "cial: {}", e.what());
            return 1;
        }
        return 0;
    }

    if(std::string(argv[1]) == "--jit" || std::string(argv[1]) == "--trace-jit") {
        if(argc != 3) {
            fmt::println(stderr, "Usage: cial --jit|--trace-jit <script-file>");
            return 1;
        }
        try {
            vmState.setJitMode(std::string(argv[1]) == "--jit" ? vm::JitMode::Eager : vm::JitMode::Trace);
            vm.eval(String(argv[2]), true);
            const auto stats = vmState.jitStats();
            fmt::println(stderr, "[jit] compiled={} executions={} type-exits={} side-exits={} traces={}",
                         stats.compiled, stats.executions, stats.typeExits, stats.sideExits, stats.recordedTraces);
        } catch(const std::exception &e) {
            fmt::println(stderr, "cial: {}", e.what());
            return 1;
        }
        return 0;
    }

    if(std::string(argv[1]) == "--trace-execution") {
        if(argc != 4) {
            fmt::println(stderr, "Usage: cial --trace-execution <script-file> <output.dot>");
            return 1;
        }
        std::ofstream output{ argv[3] };
        if(!output) {
            fmt::println(stderr, "cial: cannot open trace output: {}", argv[3]);
            return 1;
        }
        vm::ExecutionTrace trace;
        vmState.setExecutionTrace(&trace);
        int status = 0;
        try {
            vm.eval(String(argv[2]), true);
        } catch(const std::exception &e) {
            fmt::println(stderr, "cial: runtime error: {}", e.what());
            status = 1;
        }
        vmState.setExecutionTrace(nullptr);
        output << trace.dumpDot();
        output.close();
        if(!output) {
            fmt::println(stderr, "cial: cannot write trace output: {}", argv[3]);
            return 1;
        }
        return status;
    }

    try {
        vm.eval(String(argv[1]), true);
    } catch(const std::exception &e) {
        // 脚本运行期异常(未捕获 throw、未定义标识符等)——报告而非静默终止
        fmt::println(stderr, "cial: runtime error: {}", e.what());
        return 1;
    }

    // CIAL_DUMP=1:重跑编译管线打印 TAC 指令(调试用)
    if(const char *dp = std::getenv("CIAL_DUMP"); dp && *dp) {
        Common::Result r{};
        Common::SourceFile sf{ argv[1] };
        sf.load(r);
        syntax::Parser parser{ sf, context.pp() };
        syntax::AstNode *node = parser.parse(r);
        inter::IRGenerator codeGen{ r, rt, sf };
        OptReg ir{};
        const auto chunk = codeGen.parseAst(node, ir);
        if(chunk)
            fmt::println("--- dump ---\n{}", chunk->dumpInstructions());
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = duration_cast<std::chrono::nanoseconds>(end - start);

    fmt::println("Time taken by function: {}ns", duration.count());

    return 0;
}
