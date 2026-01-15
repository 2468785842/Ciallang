//
// Created by LiDong on 2026/1/15.
//

#include <catch.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("OOP - 构造函数调用与参数传递") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            var x;
            function A(v) { x = v; }
        }
        var o = new A(42);
        var res = o.x;
    )"_str);

    // 构造函数参数应正确传入
    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 42);
}

TEST_CASE("OOP - 成员变量初始化发生在构造函数之前") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            var x = 10;
            function A() { x += 5; }
        }
        var o = new A();
        var res = o.x;
    )"_str);

    // 先初始化为 10，再被构造函数修改
    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 15);
}

TEST_CASE("OOP - 成员属性 getter 行为") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            var x = 3;
            property p {
                getter() { return x * 2; }
            }
        }
        var o = new A();
        var res = o.p;
    )"_str);

    // 属性 getter 应被调用
    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 6);
}
