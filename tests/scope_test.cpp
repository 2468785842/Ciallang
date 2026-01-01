/*
 * Copyright (c) 2026/1/1 上午9:14
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

#include "test_config.h"

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

#include "types/Function.hpp"

using namespace Cial;

TEST_CASE("作用域 - 上层访问") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    const VM vm{ &vmState };
    const auto r = vm.eval<Integer>(R"(
        var r = 1;
        var a = 5;
        function outer() {
            var a = 10;
            function inner() {
                r = a;
            }
            inner();
        }
        outer();
        return r;
    )"_str);
    REQUIRE(*r == 5);
}
//
// TEST_CASE("解释器 - 执行测试") {
//     Runtime rt{};
//     Context context{ rt };
//     context.registryFunc("println"_str, &StdLib::S_PrintlnFunction);
//     Bytecode::VMState vmState{ context };
//     const VM vm{ &vmState };
//     vm.eval(String(TEST_FILES_PATH R"(/startup.tjs)"), true);
// }
//
// TEST_CASE("解释器 - 脚本执行性能") {
//     BENCHMARK("fib 15") {
//         Runtime rt{};
//         Context context{ rt };
//         Bytecode::VMState vmState{ context };
//         const VM vm{ &vmState };
//         vm.eval(R"(
//             function fib(n) {
//                 if(n < 2) return n;
//                 return fib(n - 2) + fib(n - 1);
//             }
//             fib(15);
//         )"_str);
//     };
// }