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

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace Cial;

TEST_CASE("作用域 - 上层访问") {
    // NOTE: TJS2 doesn't support accessing local variables of parent functions
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

TEST_CASE("作用域 - 上下文静态变量访问") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    const VM vm{ &vmState };

    auto r = vm.eval<Integer>(R"(
        var x = 1;

        class Derived {
            function getX() {
                // If x is in the local scope, use x;
                // Else dynamically look it up in `this`, `super`, or the global scope.
                // NOTE: So how would a context mechanism like `incontextof` change classes?
                // NOTE: Perhaps nothing would happen;
                // NOTE: maybe context mechanisms are not applicable to class scenarios.
                return x;
            }
        }

        // old TJS2 will be Failed use `return (new Derived()).getX();`
        return new Derived().getX();
    )"_str);

    REQUIRE(*r == 1);
    // TODO:
    //     r = vm.eval<Integer>(R"(
    //         var x = 1;
    //
    //         class Derived {
    //             var x = 2;
    //             function getX() { return x; }
    //         }
    //
    //         return new Derived().getX();
    //     )"_str);
    //     REQUIRE(*r == 2);
}

//
// TEST_CASE("作用域 - 父类静态变量访问") {
//     Runtime rt{};
//     Context context{ rt };
//     Bytecode::VMState vmState{ context };
//     const VM vm{ &vmState };
//     const auto r = vm.eval<Integer>(R"(
//         class Base {}
//         Base.a = 1;
//         class Derived extends Base {
//             function getSuperA() { return a; }
//         }
//         return (new Derived()).getSuperA();
//     )"_str);
//     REQUIRE(*r == 5);
// }
