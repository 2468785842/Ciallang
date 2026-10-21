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

#include "stdlib/Print.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

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
    const VM vm{ &vmState };
    try {
        vm.eval(String(argv[1]), true);
    } catch(const std::exception &e) {
        // 脚本运行期异常(未捕获 throw、未定义标识符等)——报告而非静默终止
        fmt::println(stderr, "cial: runtime error: {}", e.what());
        return 1;
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = duration_cast<std::chrono::nanoseconds>(end - start);

    fmt::println("Time taken by function: {}ns", duration.count());

    return 0;
}
