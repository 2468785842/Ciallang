#define _CRT_SECURE_NO_WARNINGS

#include "Application.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "Compilation.hpp"
#include "Diagnostics.hpp"
#include "Repl.hpp"
#include "Session.hpp"
#include "cial/Capability.hpp"
#include "common/Colorizer.hpp"
#include "common/Defer.hpp"
#include "decl/DeclModel.hpp"
#include "decompile/Decompiler.hpp"
#include "stdlib/ExceptionLib.hpp"
#include "stdlib/ScriptsLib.hpp"
#include "stdlib/StandardLibrary.hpp"
#include "tjsbc/TjsBytecodeReader.hpp"
#include "tjsbc/TjsTranslator.hpp"
#include "types/Class.hpp"
#include "vm/VM.hpp"

#include <algorithm>

namespace cial::cli {
    namespace {
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
                // .d.tjs 按声明模式分析（doc/declarations.md §4）：诊断走 decl.* 码；
                // 其余模式的声明输入由 compile 拒绝
                if(options.mode == Mode::CheckJson && source.isDeclaration()) {
                    decl::parseDeclarations(source, diagnostics, state.context.pp());
                    printJsonDiagnostics(diagnostics, options.jsonSchema, source.sourceName());
                    return diagnostics.isFailed() ? 1 : 0;
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
            const bool isBytecode = input.gcount() == 8 && std::memcmp(magic, tjsbc::FILE_TAG, 8) == 0;
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

        // 单行 JSON 能力报告（doc/roadmap.md §3.1），供不链接 SDK 的工具查询；
        // 消费方按 schemaVersion 判断兼容并忽略未知字段。版本串经 generate_version.py
        // 语义校验，不含需转义字符。
        int runCapabilities() {
            fmt::println(
                "{{\"schemaVersion\":{},\"sdkApiVersion\":{},\"version\":\"{}\",\"capabilities\":{{\"jit\":{}}}}}",
                cial::capabilities::capabilitySchemaVersion(), cial::capabilities::sdkApiVersion(), CIAL_VERSION_STRING,
                cial::capabilities::hasJit() ? "true" : "false");
            return 0;
        }

        std::string apiJsonString(const std::string &text) {
            std::string out;
            out.reserve(text.size() + 8);
            for(const char ch : text) {
                switch(ch) {
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
                        out += ch;
                        break;
                }
            }
            return out;
        }

        const char *apiKindName(const ApiKind kind) {
            switch(kind) {
                case ApiKind::Method:
                    return "method";
                case ApiKind::Property:
                    return "property";
                case ApiKind::Static:
                    return "static";
                case ApiKind::Global:
                    return "global";
                case ApiKind::Namespace:
                    return "namespace";
                case ApiKind::Class:
                    return "class";
            }
            return "";
        }

        void appendApiMetaJson(std::string &out, const ApiMeta &meta) {
            out += fmt::format("{{\"kind\":\"{}\",\"name\":\"{}\"", apiKindName(meta.kind), apiJsonString(meta.name));
            if(!meta.owner.empty())
                out += fmt::format(",\"owner\":\"{}\"", apiJsonString(meta.owner));
            out += ",\"variadic\":";
            out += meta.variadic ? "true" : "false";
            out += ",\"ret\":";
            out += meta.ret.empty() ? "null" : fmt::format("\"{}\"", apiJsonString(meta.ret));
            out += ",\"params\":[";
            for(size_t i = 0; i < meta.paramTypes.size(); ++i) {
                if(i)
                    out += ',';
                out += meta.paramTypes[i].empty() ? "null" : fmt::format("\"{}\"", apiJsonString(meta.paramTypes[i]));
            }
            out += ']';
            if(!meta.doc.empty())
                out += fmt::format(",\"doc\":\"{}\"", apiJsonString(meta.doc));
            out += '}';
        }

        // 注册期 API 元数据导出（doc/api-metadata.md）：仅描述注册事实，不执行用户
        // 脚本；schemaVersion 1，未知类型输出 null。Exception/Debug 为脚本侧自举类，
        // 不属原生注册，不进入导出。
        int runApiJson() {
            Runtime runtime;
            Context context{ runtime };
            vm::VMState state{ context };
            VM vm{ &state };
            // 与普通会话同一注册链（标准库 + print/println + Debug shim + Scripts）；
            // Debug shim 为脚本自举类，不属原生注册，导出仅含 API catalog。
            setupSession(context, vm, state, {});
            const auto &catalog = context.apiCatalog();
            // classes 按注册名排序保证输出确定性（typeMembers 为 unordered_map）；
            // 未登记名字的类型不进入导出。
            std::vector<std::pair<std::string, const std::vector<ApiMeta> *>> classes;
            for(const auto &[key, members] : catalog.typeMembers) {
                const auto nameIt = catalog.typeNames.find(key);
                if(nameIt != catalog.typeNames.end())
                    classes.emplace_back(nameIt->second, &members);
            }
            std::ranges::sort(classes, [](const auto &l, const auto &r) { return l.first < r.first; });
            std::string out = fmt::format("{{\"schemaVersion\":1,\"version\":\"{}\"", CIAL_VERSION_STRING);
            out += ",\"classes\":[";
            for(size_t i = 0; i < classes.size(); ++i) {
                if(i)
                    out += ',';
                out += fmt::format("{{\"name\":\"{}\",\"members\":[", apiJsonString(classes[i].first));
                for(size_t m = 0; m < classes[i].second->size(); ++m) {
                    if(m)
                        out += ',';
                    appendApiMetaJson(out, (*classes[i].second)[m]);
                }
                out += "]}";
            }
            out += "],\"globals\":[";
            for(size_t i = 0; i < catalog.globals.size(); ++i) {
                if(i)
                    out += ',';
                appendApiMetaJson(out, catalog.globals[i]);
            }
            out += "]}\n";
            fmt::print("{}", out);
            return 0;
        }

        // --api-decl（doc/declarations.md §3）：注册事实 → .d.tjs 声明文本。
        // 与 --api-json 同一注册链（标准库 + print/println + Scripts），不执行用户脚本；
        // 输出仅由模型决定，宿主可经 decl::declarationsFromCatalog +
        // decl::generateDeclarations 导出自己的声明
        int runApiDecl() {
            Runtime runtime;
            Context context{ runtime };
            vm::VMState state{ context };
            VM vm{ &state };
            setupSession(context, vm, state, {});
            fmt::print("{}", decl::generateDeclarations(decl::declarationsFromCatalog(context.apiCatalog())));
            return 0;
        }
    } // namespace

    int runApplication(const Options &options) {
        const auto start = std::chrono::steady_clock::now();
        if(options.noColor)
            Common::G_ColorEnabled = false;
        if(options.mode == Mode::Capabilities)
            return runCapabilities();
        if(options.mode == Mode::ApiJson)
            return runApiJson();
        if(options.mode == Mode::ApiDecl)
            return runApiDecl();
        Runtime runtime;
        Context context{ runtime };
        vm::VMState state{ context };
        VM vm{ &state };
        const bool compileOnly = options.mode == Mode::CheckJson || options.mode == Mode::DumpCfg ||
            options.mode == Mode::DumpSsa || options.mode == Mode::Decompile;
        if(!compileOnly)
            setupSession(context, vm, state,
                         options.input.empty() ? std::filesystem::path{} : std::filesystem::path{ options.input });
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
