/*
 * Copyright (c) 2024/5/29 下午5:45
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
// cial-dbg：TJS2 交互式调试器（终端 CLI）。用法：cial-dbg <script.tjs>。
// 断点/单步/debugger 语句停机后进入命令提示符，见 doc/debugger.md。

#include <argparse/argparse.hpp>

#include <exception>
#include <filesystem>
#include <string>

#include "cial/Version.hpp"
#include "cli/Compilation.hpp"
#include "cli/Diagnostics.hpp"
#include "cli/Session.hpp"
#include "common/SourceFile.hpp"
#include "dbg/DebugSession.hpp"
#include "fmt/format.h"
#include "types/Class.hpp"
#include "vm/VM.hpp"

int main(const int argc, char **argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    argparse::ArgumentParser parser{ "cial-dbg", CIAL_VERSION_STRING };
    parser.add_description("TJS2 interactive terminal debugger (see doc/debugger.md).");
    parser.add_argument("script").help("script to debug (.tjs)");
    try {
        parser.parse_args(argc, argv);
    } catch(const std::exception &error) {
        fmt::println(stderr, "cial-dbg: {}", error.what());
        return 2;
    }
    const std::string arg = parser.get<std::string>("script");

    // 调试会话强制走解释器：JIT 会整段跳过派发环，绕过 DebugHook
    cial::Runtime runtime;
    cial::Context context{ runtime };
    cial::vm::VMState state{ context };
    cial::VM vm{ &state };
    cial::cli::setupSession(context, vm, state, std::filesystem::path{ arg });

    cial::Common::Result diagnostics;
    cial::Common::SourceFile source{ std::filesystem::path{ arg } };
    source.load(diagnostics);
    auto compilation = cial::cli::compile(diagnostics, context, source);
    cial::cli::printDiagnostics(diagnostics);
    if(diagnostics.isFailed() || !compilation.chunk)
        return 1;

    // 入口 Chunk 经 RootGuard 保 GC 根贯穿会话；函数 Chunk 经常量图保持可达
    auto rooted = std::move(*compilation.chunk).toHeap(state.rt);
    const auto depth = state.context.stackTop();
    try {
        state.allocCallFrame(rooted.get());
        state.makeClosure();
        state.curFrame()->setThisObj(cial::Value{ static_cast<cial::Object *>(state.context.global()) });
        state.resetFault(); // 每轮执行重置，避免上一轮故障位置污染本次输出

        cial::dbg::DebugSession session{ state, *rooted.get() };
        state.setDebugHook(&session);
        if(!session.initialPrompt()) {
            state.setDebugHook(nullptr);
            return 0;
        }
        state.run();
        state.setDebugHook(nullptr);
        fmt::println("program finished");
        return 0;
    } catch(const cial::vm::DebugAbort &) {
        state.setDebugHook(nullptr);
        while(state.context.stackTop() > depth)
            state.freeCallFrame();
        return 0;
    } catch(const std::exception &error) {
        state.setDebugHook(nullptr);
        while(state.context.stackTop() > depth)
            state.freeCallFrame();
        state.clearException();
        cial::cli::printRuntimeError(state, error.what());
        return 1;
    }
}
