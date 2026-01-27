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

    const auto start = std::chrono::high_resolution_clock::now();
    Runtime rt{};
    Context context{ rt };
    context.registryGlobalFunc("print"_str, &stdlib::S_PrintFunction);
    context.registryGlobalFunc("println"_str, &stdlib::S_PrintlnFunction);
    vm::VMState vmState{ context };
    const VM vm{ &vmState };
    vm.eval(String(argv[1]), true);

    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = duration_cast<std::chrono::nanoseconds>(end - start);

    fmt::println("Time taken by function: {}ns", duration.count());

    return 0;
}
