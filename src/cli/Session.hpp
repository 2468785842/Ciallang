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

#include <filesystem>

#include "runtime/Context.hpp"
#include "vm/VMState.hpp"

namespace cial {
    class VM;
}

namespace cial::cli {
    // CLI 与调试工具共享的会话初始化：注册标准库、print/println、Debug shim、
    // 异常类，并重绑定 Scripts lib。scriptPath 为当前执行脚本路径（空表示
    // stdin/无文件）；Scripts 存储基目录取其所在目录（相对路径相对脚本目录，
    // krkr2 TVPSearchPlacedPath 的 CLI 语义）
    void setupSession(Context &context, VM &vm, vm::VMState &state, const std::filesystem::path &scriptPath);
} // namespace cial::cli
