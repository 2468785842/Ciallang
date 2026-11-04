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
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "vm/DebugHook.hpp"
#include "vm/VMState.hpp"

namespace cial::vm {
    class Chunk;
}

namespace cial::dbg {
    // gdb 风格终端调试会话：vm::DebugHook 实现。断点/单步命中或 debugger 语句
    // 到达时同步进入命令提示符（阻塞读 stdin），continue/step 等恢复命令返回后
    // VM 继续执行。单线程同步模型：停止期间 VM 不运行，GC 无并发。
    // 断点站点保存 Chunk*：入口 Chunk 由调用方 RootGuard 保 GC 根，函数 Chunk
    // 经常量图（FuncMeta::chunk）保持可达。
    class DebugSession final : public vm::DebugHook {
    public:
        DebugSession(vm::VMState &state, vm::Chunk &entryChunk);
        ~DebugSession() override = default;

        // 首次 run 前的入口提示符（已停帧、未执行）；返回 false 表示退出不执行
        bool initialPrompt();

        void onInstruction(vm::VMState &state, CallFrame &frame, u64 pc) override;
        // 脚本 debugger 语句：无论当前模式一律停机
        void onDebugger(vm::VMState &state, CallFrame &frame, u64 pc) override;

    private:
        enum class Mode {
            Initial, // 入口提示符阶段（尚未执行任何指令）
            Running, // 仅断点
            Step, // 行步进（进入调用）
            Next, // 行步过（不进入调用）
            Finish, // 步出：等当前帧返回
        };

        struct Breakpoint {
            int number{};
            int requestLine{}; // 用户请求行，1 基
            int line{}; // 实际吸附行，1 基；0 = 未解析到代码
            std::vector<std::pair<vm::Chunk *, u64>> sites;
        };

        void enterPrompt(CallFrame &frame, u64 pc);
        // 执行一条用户命令；返回 true 表示要恢复执行（mode 已更新）
        bool dispatch(const std::string &input);
        void printStopBanner(CallFrame &frame, u64 pc) const;

        void cmdBreak(const std::string &args);
        void cmdDelete(const std::string &args);
        void cmdBacktrace() const;
        void cmdFrame(const std::string &args);
        void cmdLocals() const;
        void cmdPrint(const std::string &args) const;
        void cmdList(const std::string &args) const;
        void cmdDisasm(const std::string &args) const;
        void cmdHelp() const;

        // 断点解析：枚举常量图全部 Chunk，按源码行收集 bc 偏移站点；
        // 无代码行向前吸附到最近有代码行
        std::vector<vm::Chunk *> collectChunks() const;
        void resolveBreakpoint(Breakpoint &bp) const;
        [[nodiscard]] bool hitBreakpoint(CallFrame &frame, u64 pc) const;

        // 选中帧（0 = 最外层，stackTop-1 = 当前停止帧）；bt/locals/print/list
        // 均作用于该帧
        [[nodiscard]] CallFrame *selectedFrame() const;

        void printValue(const Value &value) const;
        void appendValue(std::string &out, const Value &value, int depth, const std::set<const void *> &seen) const;
        void listSourceLine(vm::Chunk &chunk, int line, const char *marker) const;

        vm::VMState &_state;
        vm::Chunk &_entryChunk;
        Mode _mode{ Mode::Initial };
        size_t _baselineDepth{}; // step/next/finish 基线（停止时的帧深度）
        int _baselineLine{}; // step/next 基线（停止时源码行，1 基；0 = 无行号）
        int _selectedFrame{};
        std::vector<Breakpoint> _breakpoints;
        int _nextBreakpointNumber{ 1 };
        std::string _lastCommand;
    };
} // namespace cial::dbg
