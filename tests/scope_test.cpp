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

#include <catch2/catch_all.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("块作用域 - 遮蔽、重新声明与退出后寄存器复用", "[scope]") {
    Runtime runtime;
    Context context{ runtime };
    vm::VMState state{ context };
    VM vm{ &state };

    SECTION("初始化先读外层，声明仅覆盖当前块") {
        REQUIRE(*vm.eval<Integer>(R"(
            function f() {
                var x=3, seen=0;
                { var x=x+4; seen=x; }
                return x*10+seen;
            }
            return f();
        )"_str) == 37);
    }

    SECTION("无初值遮蔽外层，同层重复声明重置为 void") {
        REQUIRE(*vm.eval<Integer>(R"(
            function f() {
                var x=5, y=9;
                { var x; if(x!==void) return -1; x=2; }
                var y;
                if(y!==void) return -2;
                var y=3;
                var y=y+4;
                return x*10+y;
            }
            return f();
        )"_str) == 57);
    }

    SECTION("嵌套块和函数体声明可以遮蔽参数") {
        REQUIRE(*vm.eval<Integer>(R"(
            function f(x=4) { { var x=8; } return x; }
            function g(x) { var x=x+1; return x; }
            return f()*10+g(2);
        )"_str) == 43);
    }

    SECTION("相邻块和嵌套空块退出只释放自身局部槽一次") {
        REQUIRE(*vm.eval<Integer>(R"(
            function f(p) {
                var sum=0;
                { { var a=1; sum+=a; } }
                { var b=10,c=20; b+=1; sum+=b+c; }
                { {} }
                { var c=100; sum+=c; }
                return sum+p;
            }
            return f(2);
        )"_str) == 134);
    }

    SECTION("void 初值的局部槽独立于常驻 void 槽") {
        REQUIRE(*vm.eval<Integer>(R"(
            function f() {
                var a=void,b=void;
                a=7;
                { var c=void; c=8; }
                var d=11;
                if(b!==void) return -1;
                if(void!==void) return -2;
                return a+d;
            }
            return f();
        )"_str) == 18);
    }

    SECTION("catch 参数和局部声明退出后恢复外层绑定") {
        REQUIRE(*vm.eval<Integer>(R"(
            function f() {
                var e=1, seen=0;
                try { throw 2; } catch(e) {
                    var e=3;
                    seen=e;
                }
                { var a=5,b=7; seen+=a+b; }
                return e*100+seen;
            }
            return f();
        )"_str) == 115);
    }

    SECTION("for 初始化有独立作用域，break 和 continue 保留外层变量") {
        REQUIRE(*vm.eval<Integer>(R"(
            function f() {
                var i=9,total=0;
                for(var i=0;i<5;i++) {
                    if(i==1) continue;
                    if(i==4) break;
                    total+=i;
                }
                return i*100+total;
            }
            return f();
        )"_str) == 905);
    }

    SECTION("while 和 do 块的局部声明不改变外层变量") {
        REQUIRE(*vm.eval<Integer>(R"(
            function f() {
                var x=9,total=0,i=0;
                while(i<4) {
                    var x=i;
                    i++;
                    if(x==1) continue;
                    total+=x;
                }
                do { var x=7; total+=x; } while(false);
                return x*100+total;
            }
            return f();
        )"_str) == 912);
    }
}

TEST_CASE("函数作用域 - 上层访问") {
    // NOTE: TJS2 doesn't support accessing local variables of parent functions
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
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

TEST_CASE("函数作用域 - 初始化变量") {
    // NOTE: TJS2 function default param value eval on `call` invoke time
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    const VM vm{ &vmState };
    const auto r = vm.eval<Integer>(R"(
        var a = 1;
        function outer() {
            var a = 10;
            function inner(p1 = a) {
               return p1;
            }
            a = 2;
            return inner();
        }
        return outer();
    )"_str);
    REQUIRE(*r == 1);
}

TEST_CASE("类作用域 - 上下文静态变量访问") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    const VM vm{ &vmState };

    auto r = vm.eval<Integer>(R"(
        var x = 1;

        class Derived {

            function Derived() { }

            function getX() {
                /* If x is in the local scope, use x;
                 * Else dynamically look it up in `this`, `super`, or the global scope.
                 * NOTE: So how would a context mechanism like `incontextof` change classes?
                 * NOTE: Perhaps nothing would happen;
                 * NOTE: maybe context mechanisms are not applicable to class scenarios.
                 */
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
//     vm::VMState vmState{ context };
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
