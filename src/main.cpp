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

using namespace Cial;

int main(int argc, char **argv) {
    if(argc < 2) {
        fmt::println("Usage: cial <script-file>");
        return 0;
    }

    auto start = std::chrono::high_resolution_clock::now();
    Runtime rt{};
    Context context{ rt };
    context.registryGlobalFunc("print"_str, &StdLib::S_PrintFunction);
    context.registryGlobalFunc("println"_str, &StdLib::S_PrintlnFunction);
    Bytecode::VMState vmState{ context };
    const VM vm{ &vmState };
    vm.eval(String(argv[1]), true);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::nanoseconds>(end - start);

    fmt::println("Time taken by function: {}ns", duration.count());

    return 0;
}
