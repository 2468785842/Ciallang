#define _CRT_SECURE_NO_WARNINGS

#include "Application.hpp"
#include "stdlib/StandardLibrary.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "Compilation.hpp"
#include "Diagnostics.hpp"
#include "Repl.hpp"
#include "common/Colorizer.hpp"
#include "common/Defer.hpp"
#include "decompile/Decompiler.hpp"
#include "stdlib/ExceptionLib.hpp"
#include "stdlib/Print.hpp"
#include "stdlib/ScriptsLib.hpp"
#include "tjsbc/TjsBytecodeReader.hpp"
#include "tjsbc/TjsTranslator.hpp"
#include "types/Class.hpp"
#include "vm/VM.hpp"

namespace cial::cli {
    namespace {
        void initializeSession(Context &context, VM &vm, vm::VMState &state, const Options &options) {
            stdlib::registerStandardLibrary(context);
            context.registryGlobalFunc("print"_str, &stdlib::S_PrintFunction);
            context.registryGlobalFunc("println"_str, &stdlib::S_PrintlnFunction);
            vm.eval(R"(class __DebugShim {
    function message(s) { println(s); }
}
var Debug = new __DebugShim();
)"_str);
            stdlib::initExceptionClass(vm);
            // 重绑定 Scripts：嵌套执行所需的 VMState + 存储基目录（相对路径
            // 相对当前执行脚本所在目录，krkr2 TVPSearchPlacedPath 的 CLI 语义）
            stdlib::registerScriptsLib(context, &state,
                                       options.input.empty() ? std::filesystem::path{}
                                                             : std::filesystem::path{ options.input }.parent_path());
        }

        int runSource(const Options &options, vm::VMState &state) {
            Common::Result diagnostics;
            // `-` 从 stdin 读取；诊断身份用逻辑名（默认 <stdin>），不暴露宿主临时路径
            const bool fromStdin = options.input == "-";
            Common::SourceFile source{ fromStdin ? std::filesystem::path{} : std::filesystem::path{ options.input } };
            source.setSourceName(fromStdin && options.sourceName.empty() ? "<stdin>" : options.sourceName);
            Compilation compilation;
            try {
                if(fromStdin) {
                    std::string content{ std::istreambuf_iterator<char>{ std::cin }, std::istreambuf_iterator<char>{} };
                    source.load(diagnostics, content);
                } else {
                    source.load(diagnostics);
                }
                compilation = compile(diagnostics, state.context, source);
            } catch(const std::exception &error) {
                diagnostics.error(error.what());
            }
            if(options.mode == Mode::CheckJson) {
                printJsonDiagnostics(diagnostics, options.jsonSchema, source.sourceName());
                return diagnostics.isFailed() ? 1 : 0;
            }
            printDiagnostics(diagnostics);
            if(diagnostics.isFailed() || !compilation.chunk)
                return 1;
            if(options.mode == Mode::DumpCfg || options.mode == Mode::DumpSsa) {
                dumpGraphs(state.rt, *compilation.chunk, options.mode == Mode::DumpSsa);
                return 0;
            }
            const char *dump = std::getenv("CIAL_DUMP");
            if(options.dumpTac || (dump && *dump))
                fmt::println(stderr, "{}", compilation.chunk->dumpInstructions());
            execute(state, std::move(*compilation.chunk));
            return 0;
        }

        int runBytecode(const Options &options, vm::VMState &state) {
            const auto file = tjsbc::readTjsBytecodeFile(options.input);
            tjsbc::TjsTranslator translator{ state.rt, file };
            const auto program = translator.translate();
            if(program.topLevel < 0 || static_cast<size_t>(program.topLevel) >= program.contexts.size())
                throw std::runtime_error("bytecode has no top-level context");
            auto *chunk = program.contexts[static_cast<size_t>(program.topLevel)].chunk;
            if(!chunk)
                throw std::runtime_error("bytecode top-level context has no code");
            if(const char *dump = std::getenv("CIAL_TJSBC_DUMP"); dump && *dump) {
                if(std::string(dump) == "all") {
                    for(size_t i = 0; i < program.contexts.size(); ++i)
                        if(program.contexts[i].chunk)
                            fmt::println(stderr, "// context {}\n{}", i, program.contexts[i].chunk->dumpInstructions());
                } else {
                    fmt::println(stderr, "{}", chunk->dumpInstructions());
                }
            } else if(options.dumpTac) {
                fmt::println(stderr, "{}", chunk->dumpInstructions());
            }
            const auto depth = state.context.stackTop();
            DEFER {
                while(state.context.stackTop() > depth)
                    state.freeCallFrame();
            };
            state.allocCallFrame(chunk);
            state.makeClosure();
            state.curFrame()->setThisObj(Value{ static_cast<Object *>(state.context.global()) });
            state.resetFault(); // 每轮执行重置，避免上一轮故障位置污染本次输出
            try {
                state.run();
            } catch(...) {
                state.clearException();
                throw;
            }
            return 0;
        }
        // --decompile：输入按文件头探测（"TJS2 100\0" → 字节码路径，否则源码路径），
        // 严格模式默认（M0 全部 Unsupported：逐单元诊断、退出码 2、不产出文本）；
        // --view 输出反汇编级诊断视图，--map 写行表 JSON，--source 做偏移→行号换算
        int runDecompile(const Options &options, vm::VMState &state) {
            std::ifstream input{ options.input, std::ios::binary };
            if(!input)
                throw std::runtime_error("cannot open input: " + options.input);
            char magic[8]{};
            input.read(magic, sizeof(magic));
            // 与 Reader::FILE_TAG 同串：'T','J','S','2','1','0','0',0（第 5 字节 '1'，无空格）
            constexpr char tjs2Magic[8] = { 'T', 'J', 'S', '2', '1', '0', '0', '\0' };
            const bool isBytecode = input.gcount() == 8 && std::memcmp(magic, tjs2Magic, 8) == 0;
            input.close();

            Common::SourceFile originalSource;
            const bool hasSource = !options.decompileSource.empty();
            if(hasSource) {
                Common::Result result;
                originalSource = Common::SourceFile{ std::filesystem::path{ options.decompileSource } };
                originalSource.load(result);
                if(result.isFailed())
                    throw std::runtime_error("cannot load --source: " + options.decompileSource);
            }
            decompile::Options decompileOptions{ .diagnosticView = options.decompileView,
                                                 .originalSource = hasSource ? &originalSource : nullptr };

            decompile::DecompileResult decompiled;
            if(isBytecode) {
                const auto file = tjsbc::readTjsBytecodeFile(options.input);
                tjsbc::TjsTranslator translator{ state.rt, file };
                decompiled = decompile::decompileProgram(state.rt, translator.translate(), decompileOptions);
            } else {
                Common::Result diagnostics;
                Common::SourceFile source{ std::filesystem::path{ options.input } };
                source.load(diagnostics);
                if(diagnostics.isFailed()) {
                    printDiagnostics(diagnostics);
                    return 1;
                }
                const Compilation compilation = compile(diagnostics, state.context, source);
                printDiagnostics(diagnostics);
                if(diagnostics.isFailed() || !compilation.chunk)
                    return 1;
                decompiled = decompile::decompileChunk(state.rt, *compilation.chunk, decompileOptions);
            }

            if(!options.decompileMap.empty()) {
                std::ofstream map{ options.decompileMap, std::ios::binary };
                if(!map)
                    throw std::runtime_error("cannot write --map output: " + options.decompileMap);
                map << decompiled.mapJson();
            }
            if(decompiled.status != decompile::Status::Success && !options.decompileView) {
                for(const auto &diagnostic : decompiled.diagnostics)
                    fmt::println(stderr, "cial: decompile unsupported: {}", diagnostic);
                fmt::println(stderr,
                             "cial: decompile not supported for this input yet (use --view for the "
                             "disassembly-level diagnostic view)");
                return 2;
            }
            fmt::print("{}", decompiled.text());
            return 0;
        }
    } // namespace

    int runApplication(const Options &options) {
        const auto start = std::chrono::steady_clock::now();
        if(options.noColor)
            Common::G_ColorEnabled = false;
        Runtime runtime;
        Context context{ runtime };
        vm::VMState state{ context };
        VM vm{ &state };
        const bool compileOnly = options.mode == Mode::CheckJson || options.mode == Mode::DumpCfg ||
            options.mode == Mode::DumpSsa || options.mode == Mode::Decompile;
        if(!compileOnly)
            initializeSession(context, vm, state, options);
        state.setJitMode(options.jit);
        state.setTailRecursionEnabled(options.tailRecursion);

        vm::ExecutionTrace trace;
        std::ofstream output;
        if(!options.traceOutput.empty()) {
            if(std::filesystem::weakly_canonical(options.input) ==
               std::filesystem::weakly_canonical(options.traceOutput))
                throw std::runtime_error("trace output must differ from the input file");
            output.open(options.traceOutput);
            if(!output)
                throw std::runtime_error("cannot open trace output: " + options.traceOutput);
            state.setExecutionTrace(&trace);
        }
        int status = 0;
        try {
            if(options.mode == Mode::Repl)
                status = runRepl(state, std::cin, isInteractiveInput());
            else if(options.mode == Mode::Bytecode)
                status = runBytecode(options, state);
            else if(options.mode == Mode::Decompile)
                status = runDecompile(options, state);
            else
                status = runSource(options, state);
        } catch(const std::exception &error) {
            // 运行期异常：VM 只携带异常值，文件与源码位置（如有）在此附加
            printRuntimeError(state, error.what());
            status = 1;
        }
        state.setExecutionTrace(nullptr);
        if(output.is_open()) {
            output << trace.dumpDot();
            output.close();
            if(!output) {
                fmt::println(stderr, "cial: cannot write trace output: {}", options.traceOutput);
                status = 1;
            }
        }
        if(options.tailRecursion)
            fmt::println(stderr, "[tail] eliminated={}", state.tailCallsEliminated());
        if(options.jit != vm::JitMode::Off) {
            const auto stats = state.jitStats();
            fmt::println(stderr, "[jit] compiled={} executions={} type-exits={} side-exits={} traces={}",
                         stats.compiled, stats.executions, stats.typeExits, stats.sideExits, stats.recordedTraces);
        }
        if(options.timing) {
            const auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start);
            fmt::println(stderr, "Time taken by function: {}ns", duration.count());
        }
        return status;
    }
} // namespace cial::cli
