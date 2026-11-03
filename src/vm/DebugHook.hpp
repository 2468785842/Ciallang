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

#include "types/Types.hpp"

// CallFrame 定义在 cial（runtime/Context.hpp 以 struct CallFrame 前向声明），
// 这里声明同型前置，不引入 vm/CallFrame.hpp
namespace cial {
    struct CallFrame;
}

namespace cial::vm {
    class VMState;

    // 调试器中途放弃执行：刻意不继承 std::exception——runLoop 的
    // catch(const std::exception &) 会把 std 异常包装成脚本 Exception，
    // 普通类型异常才能从 hook 原样穿透 run() 到调用方收尾（栈平衡后退出）
    struct DebugAbort {};

    // 每条指令边界同步接管的调试钩子：hook 内可阻塞交互（读命令），
    // 返回即恢复执行该指令。借用语义，同 ExecutionTrace（setDebugHook）。
    // 调试会话必须关闭 JIT（JIT tryRun 会整段跳过派发环，绕过钩子）。
    class DebugHook {
    public:
        virtual ~DebugHook() = default;

        // 每条指令派发前调用；pc 为该指令起始的扁平字节码偏移
        virtual void onInstruction(VMState &state, CallFrame &frame, u64 pc) = 0;

        // 脚本 debugger 语句命中；默认按普通指令边界处理
        virtual void onDebugger(VMState &state, CallFrame &frame, u64 pc) { onInstruction(state, frame, pc); }
    };
} // namespace cial::vm
