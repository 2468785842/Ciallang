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

// TODO:
// TEST_CASE("OOP - instanceof Class 判断") {
//     Runtime rt{};
//     Context context{ rt };
//     Bytecode::VMState vmState{ context };
//     VM vm{ &vmState };
//
//     vm.eval(R"(
//         class A { }
//         var o = new A();
//         var res = (A instanceof "Class") && (o instanceof A);
//     )"_str);
//
//     REQUIRE(*vm.evalExpr<Integer>("res"_str) == 1);
// }

// TODO:
// TEST_CASE("OOP - 类方法内 new 必须使用 global.ClassName") {
//     Runtime rt{};
//     Context context{ rt };
//     Bytecode::VMState vmState{ context };
//     VM vm{ &vmState };
//
//     vm.eval(R"(
//         class A {
//             function A() {}
//             function create() {
//                 return new global.A();
//             }
//         }
//         var o = new A();
//         var obj2 = o.create();
//         var res = (obj2 instanceof A);
//     )"_str);
//
//     // 若未使用 global.A()，这里应失败
//     REQUIRE(*vm.evalExpr<Integer>("res"_str) == 1);
// }

// TODO:
// TEST_CASE("OOP - invalidate 调用 finalize") {
//     Runtime rt{};
//     Context context{ rt };
//     Bytecode::VMState vmState{ context };
//     VM vm{ &vmState };
//
//     vm.eval(R"(
//         var flag = 0;
//         class A {
//             function finalize() {
//                 flag = 1;
//             }
//         }
//         var o = new A();
//         invalidate o;
//     )"_str);
//
//     // finalize 应在 invalidate 时被调用
//     REQUIRE(*vm.evalExpr<Integer>("flag"_str) == 1);
// }

// TODO:
// TEST_CASE("OOP - isvalid 在 invalidate 前后行为") {
//     Runtime rt{};
//     Context context{ rt };
//     Bytecode::VMState vmState{ context };
//     VM vm{ &vmState };
//
//     vm.eval(R"(
//         class A {}
//         var o = new A();
//         var before = isvalid o;
//         invalidate o;
//         var after = isvalid o;
//     )"_str);
//
//     REQUIRE(*vm.evalExpr<Integer>("before"_str) == 1);
//     REQUIRE(*vm.evalExpr<Integer>("after"_str) == 0);
// }

TEST_CASE("OOP - 方法闭包保持对象上下文") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            var x = 1;
            function inc() { x++; }
        }
        var o = new A();
        var f = o.inc;
        f();
        var res = o.x;
    )"_str);

    // 即使脱离对象调用，仍应操作原对象
    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

// TODO:
// TEST_CASE("OOP - incontextof 修改闭包上下文") {
//     Runtime rt{};
//     Context context{ rt };
//     Bytecode::VMState vmState{ context };
//     VM vm{ &vmState };
//
//     vm.eval(R"(
//         class A {
//             var x = 0;
//             function inc() { x++; }
//         }
//         var a1 = new A();
//         var a2 = new A();
//         var f = a1.inc;
//         (f incontextof a2)();
//         var res1 = a1.x;
//         var res2 = a2.x;
//     )"_str);
//
//     REQUIRE(*vm.evalExpr<Integer>("res1"_str) == 0);
//     REQUIRE(*vm.evalExpr<Integer>("res2"_str) == 1);
// }

TEST_CASE("OOP - 单继承与 super 方法调用") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            function foo() { return 1; }
        }
        class B extends A {
            function foo() { return super.foo() + 1; }
        }
        var o = new B();
        var res = o.foo();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("OOP - override 隐藏父类方法") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            function foo() { return 1; }
        }
        class B extends A {
            function foo() { return 2; }
        }
        var o = new B();
        var res = o.foo();
    )"_str);

    // 子类方法应隐藏父类方法
    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("OOP - 多重继承方法覆盖顺序") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A { function foo() { return 1; } }
        class B { function foo() { return 2; } }
        class C extends A, B { }
        var o = new C();
        var res = o.foo();
    )"_str);

    // 后继承的类优先
    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("OOP - 子类成员变量覆盖父类成员变量") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A { var x = 1; }
        class B extends A { var x = 2; }
        var o = new B();
        var res = o.x;
    )"_str);

    // 成员变量不是 override，而是直接覆盖
    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}
