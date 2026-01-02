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
