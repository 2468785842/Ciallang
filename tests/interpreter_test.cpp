/*
 * Copyright (c) 2024/5/30 上午9:14
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

#include <catch.hpp>

#include "stdlib/Print.hpp"
#include "test_config.h"

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

#include "types/Function.hpp"

using namespace cial;

TEST_CASE("解释器 - Hello World") {
    Runtime rt{};
    Context context{ rt };
    context.registryGlobalFunc("println"_str, &StdLib::S_PrintlnFunction);
    Bytecode::VMState vmState{ context };
    const VM vm{ &vmState };
    vm.evalExpr(R"(println("Hello World!"))"_str);
}

TEST_CASE("解释器 - 执行测试") {
    Runtime rt{};
    Context context{ rt };
    context.registryGlobalFunc("println"_str, &StdLib::S_PrintlnFunction);
    Bytecode::VMState vmState{ context };
    const VM vm{ &vmState };
    vm.eval(String(TEST_FILES_PATH R"(/startup.tjs)"), true);
}

TEST_CASE("解释器 - 脚本执行性能") {
    // @1    : cp         %0   %1
    // @2    : load       %2   *1
    // @3    : lt         %1   %2
    // @4    : test       %2
    // @5    : jmp_ne     @8
    // @6    : cp         %0   %3
    // @7    : ret        %3
    // @8    : g_this     atom_5 %4
    // @9    : cp         %0   %5
    // @10   : load       %6   *1
    // @11   : sub        %5   %6
    // @12   : push_reg   %6
    // @13   : call       %4   %2   1
    // @14   : g_this     atom_5 %7
    // @15   : cp         %0   %8
    // @16   : load       %9   *2
    // @17   : sub        %8   %9
    // @18   : push_reg   %9
    // @19   : call       %7   %4   1
    // @20   : add        %2   %4
    // @21   : ret        %4
    BENCHMARK("fib 10") {
        Runtime rt{};
        Context context{ rt };
        Bytecode::VMState vmState{ context };
        const VM vm{ &vmState };
        vm.eval(R"(
            function fib(n) {
                if(n < 2) return n;
                return fib(n - 2) + fib(n - 1);
            }
            fib(10);
        )"_str);
    };
}