//
// Created by LiDong on 2025/12/6.
//

#include <catch.hpp>

#include "types/Class.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("OOP - 类声明与实例化") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };
    const auto r = vm.eval<InstanceObject *>(R"(
        class A { }
        var o = new A();
        return o;
    )"_str);
    REQUIRE(*r);
}

TEST_CASE("OOP - 类方法定义与调用") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("实例方法调用") {
        vm.eval(R"(
            class A { function add(x, y) { return x + y; } }
            var o = new A();
            var res = o.add(1, 2);
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("res"_str) == 3);
    }

    SECTION("实例成员访问") {
        vm.eval(R"(
            class A { var a = 1; }
            var o = new A();
            var res = o.a;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("res"_str) == 1);
    }
}

TEST_CASE("OOP - 类访问变量") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("实例成员初始化变量访问") {
        vm.eval(R"(
            var a = 1;
            class A {
                var b = a;
            }
            var o = new A().b;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("o"_str) == 1);
    }

    SECTION("实例成员方法初始化变量访问") {
        vm.eval(R"(
            var a = 1;
            class A {
                function b() { return a; }
            }
            var o = new A().b();
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("o"_str) == 1);
    }

    SECTION("实例成员函数访问成员变量") {
        vm.eval(R"(
            class A {
                var a = 1;
                function b() { return a; }
            }
            var o = new A().b();
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("o"_str) == 1);
    }
}
