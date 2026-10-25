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
    vm::VMState vmState{ context };
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
    vm::VMState vmState{ context };
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
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            var x = 3;
            function A() {}
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

TEST_CASE("OOP - instanceof Class 判断") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A { function A() {} }
        var o = new A();
        var res = (A instanceof "Class") && (o instanceof "A");
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 1);
}

TEST_CASE("OOP - 类方法内 new 必须使用 global.ClassName") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            function A() {}
            function create() {
                // We can use A() instead of global.A()
                // return new A(); // ok
                return new global.A();
            }
        }
        var o = new A();
        var obj2 = o.create();
        var res = (obj2 instanceof "A");
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 1);
}

TEST_CASE("OOP - invalidate 调用 finalize") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        var flag = 0;
        class A {
            function A() {}
            function finalize() {
                flag = 1;
            }
        }
        var o = new A();
        invalidate o;
    )"_str);

    // finalize 应在 invalidate 时被调用
    REQUIRE(*vm.evalExpr<Integer>("flag"_str) == 1);
}

TEST_CASE("OOP - isvalid 在 invalidate 前后行为") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A { function A() {} }
        var o = new A();
        var before = isvalid o;
        invalidate o;
        var after = isvalid o;
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("before"_str) == 1);
    REQUIRE(*vm.evalExpr<Integer>("after"_str) == 0);
}

TEST_CASE("OOP - 方法闭包保持对象上下文") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            var x = 1;
            function A() {}
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

TEST_CASE("OOP - incontextof 修改闭包上下文") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            var x = 0;
            function A() {}
            function inc() { x++; }
        }
        var a1 = new A();
        var a2 = new A();
        var f = a1.inc;
        (f incontextof a2)();
        var res1 = a1.x;
        var res2 = a2.x;
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res1"_str) == 0);
    REQUIRE(*vm.evalExpr<Integer>("res2"_str) == 1);
}

TEST_CASE("OOP - 单继承与 super 方法调用") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            function A() {}
            function foo() { return 1; }
        }
        class B extends A {
            function B() { A(); }
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
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A {
            function A() {}
            function foo() { return 1; }
        }
        class B extends A {
            function B() { A(); }
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
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A { function A() {} function foo() { return 1; } }
        class B { function B() {} function foo() { return 2; } }
        class C extends A, B { function C() { A(); B(); } }
        var o = new C();
        var res = o.foo();
    )"_str);

    // 后继承的类优先
    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("OOP - 子类成员变量覆盖父类成员变量") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        class A { var x = 1; function A() {} }
        class B extends A { var x = 2; function B() { A(); } }
        var o = new B();
        var res = o.x;
    )"_str);

    // 成员变量不是 override，而是直接覆盖
    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("OOP - 父类构造函数初始化字段对子类可见") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            var x;
            function A() { x = 10; }
        }
        class B extends A {
            function B() { A(); }
        }
        var o = new B();
        var res = o.x;
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 10);
}

TEST_CASE("OOP - super 方法应作用于子类对象") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            function A() {}
            function set() { x = 5; }
        }
        class B extends A {
            var x = 1;
            function B() { A(); }
            function f() { super.set(); }
        }
        var o = new B();
        o.f();
        var res = o.x;
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 5);
}

TEST_CASE("OOP - 父子字段应共享同一 this") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            var x = 1;
            function A() {}
        }
        class B extends A {
            function B() { A(); }
            function inc() { x++; }
        }
        var o = new B();
        o.inc();
        var res = o.x;
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("TJS2 - 方法字段解析优先使用定义 class 的 slot") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            var x = 1;
            function A() {}
            function getX() { return x; }
        }
        class B extends A {
            var x = 2;
            function B() { A(); }
        }
        var o = new B();
        var res = o.getX();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("TJS2 - 字段仅在 slot 链不存在时 fallback 到 instance") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            function A() {}
            function getX() { return x; }
        }
        class B extends A {
            var x = 2;
            function B() { A(); }
        }
        var o = new B();
        var res = o.getX();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("TJS2 - super slot 优先于 instance 字段") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class C { var x = 4; function C() {} }
        class A extends C {
            function A() { C(); }
            function getX() { return x; }
            function finalize() { super.finalize(); }
        }
        class B extends A {
            var x = 2;
            function B() { A(); }
            function finalize() { super.finalize(); }
        }
        var o = new B();
        var res = o.getX();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("TJS2 - 构造函数调用会覆盖子类字段") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            var x = 1;
            function A() { x = 10; }
        }
        class B extends A {
            var x = 2;
            function B() { A(); }
        }
        var o = new B();
        var res = o.x;
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 10);
}

TEST_CASE("TJS2 - 父构造函数仅初始化父 slot") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            var x = 0;
            function A() { x = 5; }
        }
        class B extends A {
            function B() { A(); }
            function getX() { return x; }
        }
        var o = new B();
        var res = o.getX();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 5);
}

TEST_CASE("TJS2 - 多继承 slot 查找顺序") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A { var x = 1; function A() {} }
        class B { var x = 2; function B() {} }
        class C extends A, B {
            function C() { A(); B(); }
            function getX() { return x; }
        }
        var o = new C();
        var res = o.getX();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("TJS2 - 多继承 slot 不存在时 fallback 到 instance") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A { function A() {} }
        class B { function B() {} }
        class C extends A, B {
            var x = 3;
            function C() { A(); B(); }
            function getX() { return x; }
        }
        var o = new C();
        var res = o.getX();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 3);
}

TEST_CASE("TJS2 - 方法不是闭包而是绑定 slot 的函数") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            var x = 1;
            function A() {}
            function getX() { return x; }
        }
        var o = new A();
        var f = o.getX;
        var res = f();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 1);
}

TEST_CASE("TJS2 - 方法不绑定 slot 而不是调用者") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            var x = 1;
            function A() {}
            function getX() { return x; }
        }
        class B extends A {
            var x = 2;
            function B() { A(); }
        }
        var o = new B();
        var f = o.getX;
        var res = f();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 2);
}

TEST_CASE("TJS2 - 子类字段不 override 父类字段") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            var x = 1;
            function A() {}
            function getX() { return x; }
        }
        class B extends A {
            var x = 100;
            function B() { A(); }
        }
        var o = new B();
        var res = o.getX();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 100);
}

TEST_CASE("TJS2 - 多层 slot 同名字段解析") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class C { var x = 3; function C() {} }
        class B extends C { var x = 2; function B() { C(); } }
        class A extends B {
            var x = 1;
            function A() { B(); }
            function getX() { return x; }
        }
        var o = new A();
        var res = o.getX();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 1);
}

TEST_CASE("TJS2 - 仅当 slot 链完全无字段才访问 proxy") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval(R"(
        class A {
            function A(){}
            function getX() { return x; }
        }
        class B extends A {
            var x;
            function B() { A(); }
        }
        var o = new B();
        o.x = 9;
        var res = o.getX();
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("res"_str) == 9);
}

TEST_CASE("OOP - Legacy TJS2 Class/Slot/Constructor/Lookup 行为全集") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        // --- 构造函数参数传递 ---
        class A1 {
            var x;
            function A1(v) { x = v; }
        }
        var o1 = new A1(42);
        var r1 = o1.x;

        // --- 成员初始化先于构造 ---
        class A2 {
            var x = 10;
            function A2() { x += 5; }
        }
        var o2 = new A2();
        var r2 = o2.x;

        // --- getter 行为 ---
        class A3 {
            var x = 3;
            property p {
                getter() { return x * 2; }
            }
            function A3() {}
        }
        var o3 = new A3();
        var r3 = o3.p;

        // --- instanceof ---
        class A4 { function A4(){} }
        var o4 = new A4();
        var r4 = (A4 instanceof "Class") && (o4 instanceof "A4");

        // --- 方法闭包绑定 this ---
        class A5 {
            var x = 1;
            function A5() {}
            function inc() { x++; }
        }
        var o5 = new A5();
        var f5 = o5.inc;
        f5();
        var r5 = o5.x;

        // --- incontextof ---
        class A6 {
            var x = 0;
            function A6() {}
            function inc() { x++; }
        }
        var a6_1 = new A6();
        var a6_2 = new A6();
        var f6 = a6_1.inc;
        (f6 incontextof a6_2)();
        var r6_1 = a6_1.x;
        var r6_2 = a6_2.x;

        // --- override ---
        class A7 {
            function A7() {}
            function foo() { return 1; }
        }
        class B7 extends A7 {
            function B7() { A7(); }
            function foo() { return 2; }
        }
        var o7 = new B7();
        var r7 = o7.foo();

        // --- slot 查找优先定义类 ---
        class A8 {
            var x = 1;
            function A8() { }
            function getX() { return x; }
        }
        class B8 extends A8 {
            var x = 2;
            function B8() { A8(); }
        }
        var o8 = new B8();
        var r8 = o8.getX(); // == 2

        // --- fallback 到 instance ---
        class A9 {
            function A9() {}
            function getX() { return x; }
        }
        class B9 extends A9 {
            var x = 2;
            function B9() { A9(); }
        }
        var o9 = new B9();
        var r9 = o9.getX(); // == 2

        // --- super slot 优先于 instance ---
        class C10 { var x = 4; function C10() {} }
        class A10 extends C10 {
            function A10() { C10(); }
            function getX() { return x; }
        }
        class B10 extends A10 {
            var x = 2;
            function B10() { A10(); }
        }
        var o10 = new B10();
        var r10 = o10.getX(); // == 2

        // --- 构造函数不会覆盖子类字段 ---
        class A11 {
            var x = 1;
            function A11() { x = 10; }
        }
        class B11 extends A11 {
            var x = 2;
            function B11() { A11(); }
        }
        var o11 = new B11();
        var r11 = o11.x;
    )"_str);

    REQUIRE(*vm.evalExpr<Integer>("r1"_str) == 42);
    REQUIRE(*vm.evalExpr<Integer>("r2"_str) == 15);
    REQUIRE(*vm.evalExpr<Integer>("r3"_str) == 6);
    REQUIRE(*vm.evalExpr<Integer>("r4"_str) == 1);
    REQUIRE(*vm.evalExpr<Integer>("r5"_str) == 2);
    REQUIRE(*vm.evalExpr<Integer>("r6_1"_str) == 0);
    REQUIRE(*vm.evalExpr<Integer>("r6_2"_str) == 1);
    REQUIRE(*vm.evalExpr<Integer>("r7"_str) == 2);
    REQUIRE(*vm.evalExpr<Integer>("r8"_str) == 2);
    REQUIRE(*vm.evalExpr<Integer>("r9"_str) == 2);
    REQUIRE(*vm.evalExpr<Integer>("r10"_str) == 2);
    REQUIRE(*vm.evalExpr<Integer>("r11"_str) == 10);
}
TEST_CASE("TJS2 - super.<Ctor>() 显式基类构造调用") {
    // 回归：构造器内 super.<Ctor>() 曾因 _lastSuperClass 未记录而段错误
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("单继承构造链") {
        vm.eval(R"(
            class base {
                var v;
                function base() { v = 3; }
            }
            class derived extends base {
                function derived() { super.base(); }
            }
            var d = new derived();
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("d.v"_str) == 3);
    }

    SECTION("构造器内 super 调用后基类 property 可用") {
        vm.eval(R"(
            class test {
                function test() { variable = 0; }
                var variable;
                property prop {
                    getter() { return variable; }
                    setter(v) { variable = v; }
                }
            }
            class testInherited extends test {
                function testInherited() { super.test(); }
            }
            var tmp2 = new testInherited();
            tmp2.prop = 5;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("tmp2.prop"_str) == 5);
    }
}

TEST_CASE("无构造器类 new 走默认构造（原版 UB，按参考实现实测对齐）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // 无构造器 new 在原版 TJS2 为未定义行为；此处锁定参考实现实测行为：
    // 字段初始化照常（递归含基类），基类构造器不调用
    // （C 无构造器 extends B：B.b 保持字段默认值 1 而非构造器里的 2）
    vm.eval(R"tjs(
        class B { var b = 1; function B() { b = 2; } }
        class C extends B { var c = 3; }
        class D { var d = 4; }
        class E { property p { getter { return 55; } } }
        var d1 = new D();
        var dv = d1.d;
        var c1 = new C();
        var bv = c1.b;
        var cv = c1.c;
        var e1 = new E();
        var ev = e1.p;
        var eok = isvalid e1;
    )tjs"_str);
    REQUIRE(*vm.evalExpr<Integer>("dv"_str) == 4);
    REQUIRE(*vm.evalExpr<Integer>("bv"_str) == 1); // 基类构造器未执行
    REQUIRE(*vm.evalExpr<Integer>("cv"_str) == 3);
    REQUIRE(*vm.evalExpr<Integer>("ev"_str) == 55); // property 成员照常
    REQUIRE(*vm.evalExpr<Integer>("eok"_str) == 1);
}
