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

#include <crtdbg.h>

#include "stdlib/ExceptionLib.hpp"
#include "tjsbc/TjsBytecodeReader.hpp"
#include "tjsbc/TjsTranslator.hpp"
#include "stdlib/Print.hpp"
#include "vm/VM.hpp"
#include "types/Class.hpp"
#include "vm/VMState.hpp"

using namespace cial;

#ifdef _DEBUG
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

int main(const int argc, char **argv) {
    if(argc < 2) {
        fmt::println("Usage: cial <script-file>");
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
            vmState.allocCallFrame(chunk);
            vmState.makeClosure();
            // 顶层 this = 全局对象（TJS 顶层 SPDS/GPD 经 this/proxy 寄存器读写全局）
            vmState.curFrame()->setThisObj(
                Value{ static_cast<Object *>(vmState.context.global()) });
            try {
                vmState.run0();
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
