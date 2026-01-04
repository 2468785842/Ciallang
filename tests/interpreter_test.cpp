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

using namespace Cial;

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