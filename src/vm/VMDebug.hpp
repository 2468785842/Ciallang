/*
 * Copyright (c) 2026/1/1 上午8:08
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

#include "logging/Logger.hpp"

// #ifdef VM_DEBUG
#define VM_ASSERT(condition, vmState)                                                                                  \
    CLL_ASSERT(condition, R"([VM_ASSERT]
__asm__start
%s
__asm__end
PC: 0x%x
Frame depth: %d)",                                                                                                     \
               (vmState)->curFrame()->chunk->dumpInstruction().getData(), (vmState)->curFrame()->pc - 1,               \
               (vmState)->context.stackTop);
// #else
// #define VM_ASSERT(x) ((void)0)
// #endif

#if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
#if defined(_MSC_VER)
// Windows 平台 (MSVC)
#define DEBUG_BREAK() __debugbreak()
#elif defined(__clang__)
// Clang 编译器 (macOS/iOS/Linux)
#define DEBUG_BREAK() __builtin_debugtrap()
#elif defined(__GNUC__)
// GCC 编译器 (Linux/Unix)
#include <signal.h>
#if defined(__i386__) || defined(__x86_64__)
// x86 架构直接触发中断指令，比 raise 更直接
#define DEBUG_BREAK() __asm__("int $3")
#else
// 其他架构使用 POSIX 信号
#define DEBUG_BREAK() raise(SIGTRAP)
#endif
#else
// 降级方案
#include <assert.h>
#define DEBUG_BREAK() assert(false)
#endif
#else
#define DEBUG_BREAK()
#endif
