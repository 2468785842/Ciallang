/*
 * Copyright (c) 2024/12/18
 *
 * 表达式单元测试
 */

#include <catch.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("表达式 - 字面量表达式") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("整数字面量") { REQUIRE(*vm.evalExpr<Integer>("123"_str) == 123); }

    SECTION("浮点数字面量") { REQUIRE((*vm.evalExpr<Real>("3.14"_str)).value() == Catch::Approx(3.14)); }

    SECTION("字符串字面量") { REQUIRE(*(*vm.evalExpr<String *>(R"("hello world")"_str)) == "hello world"); }
}

TEST_CASE("表达式 - 顺序运算符") {
    // Note: OrderExpr is disabled in the following cases:
    // 1. Function parameter parser
    // 2. Class inheritance parser
    // 3. Array initialization parser
    // 4. Directory initialization parser
    // This is because it is the same as the tjs2 language, and there are conflicts in the syntax.
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval("var a, b, c;"_str);
    REQUIRE(*vm.evalExpr<Integer>("c = (a = 1, b = 2)"_str) == 2);
    REQUIRE(*vm.evalExpr<Integer>("c"_str) == 2);
    REQUIRE(*vm.evalExpr<Integer>("a = 1, b = 2, c = 3"_str) == 3);
}

TEST_CASE("表达式 - 三元运算符") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };
    vm.eval("var a;"_str);
    REQUIRE(*vm.evalExpr<Integer>("a ? 2 : 4"_str) == 4);
    REQUIRE(*vm.evalExpr<Integer>("a = 1"_str) == 1);
    REQUIRE(*vm.evalExpr<Integer>("a ? 2 : 4"_str) == 2);
}

TEST_CASE("表达式 - 二元运算表达式") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("算术运算") { REQUIRE((*vm.evalExpr<Integer>("1 + 2"_str)) == 3); }

    SECTION("比较运算") {
        REQUIRE((*vm.evalExpr<bool>("1 > 2"_str)) == false);
        REQUIRE((*vm.evalExpr<bool>("1 < 2"_str)) == true);
        REQUIRE((*vm.evalExpr<bool>("1 >= 2"_str)) == false);
        REQUIRE((*vm.evalExpr<bool>("2 >= 2"_str)) == true);
        REQUIRE((*vm.evalExpr<bool>("2 <= 2"_str)) == true);
        REQUIRE((*vm.evalExpr<bool>("3 <= 2"_str)) == false);
    }

    SECTION("逻辑与运算") {
        REQUIRE((*vm.evalExpr<bool>("true && false"_str)) == false);
        REQUIRE((*vm.evalExpr<bool>("true && true"_str)) == true);
    }

    SECTION("逻辑或运算") {
        REQUIRE((*vm.evalExpr<bool>("true || false"_str)) == true);
        REQUIRE((*vm.evalExpr<bool>("false || true"_str)) == true);
        REQUIRE((*vm.evalExpr<bool>("false || false"_str)) == false);
    }

    SECTION("交换运算") {
        vm.eval("var a = 1, b = 2; a <-> b;"_str);
        REQUIRE((*vm.evalExpr<Integer>("a"_str)) == 2);
        REQUIRE((*vm.evalExpr<Integer>("b"_str)) == 1);

        vm.eval("var gA, gB; { var a = 1, b = 2; a <-> b; gA = a, gB = b; }"_str);
        REQUIRE((*vm.evalExpr<Integer>("gA"_str)) == 2);
        REQUIRE((*vm.evalExpr<Integer>("gB"_str)) == 1);

        vm.eval("var a = 1, c = 3; { var b = 2; a <-> b; c = b; }"_str);
        REQUIRE((*vm.evalExpr<Integer>("a"_str)) == 2);
        REQUIRE((*vm.evalExpr<Integer>("c"_str)) == 1);
    }
}

TEST_CASE("表达式 - 一元运算表达式") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("逻辑非") {
        REQUIRE((*vm.evalExpr<bool>("!true"_str)) == false);
        REQUIRE((*vm.evalExpr<bool>("!false"_str)) == true);
    }

    SECTION("负号") {
        REQUIRE((*vm.evalExpr<Integer>("-1"_str)) == -1);
        REQUIRE((*vm.evalExpr<Integer>("-(-1)"_str)) == 1);
    }
}
TEST_CASE("表达式 - 赋值表达式") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("简单赋值") { REQUIRE((*vm.eval<Integer>("var a; a = 42; return a;"_str)) == 42); }

    SECTION("链式赋值") { REQUIRE((*vm.eval<Integer>("var a, b, c; a = b = c = 7; return a + b + c;"_str)) == 21); }

    SECTION("算术复合赋值") {
        REQUIRE((*vm.eval<Integer>("var x = 10; x += 5; return x;"_str)) == 15);
        REQUIRE((*vm.eval<Integer>("var x = 10; x -= 3; return x;"_str)) == 7);
        REQUIRE((*vm.eval<Integer>("var x = 6; x *= 7; return x;"_str)) == 42);
        REQUIRE((*vm.eval<Integer>("var x = 20; x /= 4; return x;"_str)) == 5);
        REQUIRE((*vm.eval<Integer>("var x = 17; x %= 5; return x;"_str)) == 2);
    }

    SECTION("位运算复合赋值") {
        REQUIRE((*vm.eval<Integer>("var x = 6; x &= 3; return x;"_str)) == (6 & 3));
        REQUIRE((*vm.eval<Integer>("var x = 6; x |= 3; return x;"_str)) == (6 | 3));
        REQUIRE((*vm.eval<Integer>("var x = 6; x ^= 3; return x;"_str)) == (6 ^ 3));
    }

    SECTION("逻辑复合赋值") {
        // NOTE: tjs2 will convert value types to booleans and perform || and && operations.
        // is not bug!
        // a ||= b  ≡  a = (bool(a) || bool(b))
        // a &&= b  ≡  a = (bool(a) && bool(b))
        REQUIRE((*vm.eval<bool>("var x = 0; x ||= 42; return x;"_str)) == true);

        REQUIRE((*vm.eval<bool>("var x = 1; x ||= 0; return x;"_str)) == true);

        REQUIRE((*vm.eval<bool>("var x = false; x ||= false; return x;"_str)) == false);
    }

    SECTION("移位复合赋值") {
        REQUIRE((*vm.eval<Integer>("var x = 1; x <<= 3; return x;"_str)) == 8);
        REQUIRE((*vm.eval<Integer>("var x = 8; x >>= 2; return x;"_str)) == 2);
        REQUIRE((*vm.eval<Integer>("var x = -1; x >>>= 1; return x;"_str)) >= 0);
    }

    SECTION("赋值表达式的值") {
        REQUIRE((*vm.eval<Integer>("var a = 1; var b; b = (a += 2); return a * 10 + b;"_str)) == 33);
    }

    SECTION("左值只求值一次") {
        REQUIRE((*vm.eval<Integer>("var i = 0; "
                                   "function f { i += 1; return i; } "
                                   "var a = 0; "
                                   "a += f(); "
                                   "return i;"_str)) == 1);
    }
}


TEST_CASE("表达式 - 函数表达式") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("无参数调用") { REQUIRE((*vm.evalExpr<bool>("function { return true; }()"_str)) == true); }

    SECTION("带参数调用") {
        REQUIRE((*vm.evalExpr<Integer>(R"(
            function(a, b, c) { return a + b + c; }(1, 2, 3)
        )"_str)) == 6);
    }

    SECTION("嵌套调用") {
        REQUIRE((*vm.evalExpr<Integer>(R"(
            function (a, b) { if(a < b) return b; else return a; }(function (a, b) { if(a > b) return b; else return a; }(2, 1), 3);
        )"_str)) == 3);
    }

    SECTION("逗号表示隐式void参数 - 两个逗号") {
        REQUIRE((*vm.evalExpr<bool>(R"(
            function (a, b) { return a && b; }(,)
        )"_str)) == false);
    }

    SECTION("逗号表示隐式void参数 - 三个逗号") {
        REQUIRE((*vm.evalExpr<bool>(R"(
            function (a, b, c) { return a && b && c; }(,,)
        )"_str)) == false);
    }

    SECTION("逗号表示隐式void参数 - 参数后跟逗号") {
        REQUIRE((*vm.evalExpr<bool>(R"(
            function (a, b) { return a == 2 && !b; }(2,)
        )"_str)) == true);
    }

    SECTION("逗号表示隐式void参数 - 逗号后跟参数") {
        REQUIRE((*vm.evalExpr<bool>(R"(
            function (a, b) { return !a && b == 2; }(,2)
        )"_str)) == true);
    }

    SECTION("逗号表示隐式void参数 - 混合情况") {
        REQUIRE((*vm.evalExpr<bool>(R"(
            function (a, b, c, d, e) { return a == 1 && !b && c == 2 && !d && e == 3; }(1, , 2, , 3)
        )"_str)) == true);
    }
}

TEST_CASE("表达式 - 复杂表达式组合") {
    Runtime rt{};
    Context context{ rt };
    Bytecode::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("混合表达式") { REQUIRE((*vm.evalExpr<Integer>("1 + 2 * 3 - 4 / 4"_str)) == 6); }

    SECTION("带括号的表达式") { REQUIRE((*vm.evalExpr<Integer>("(1 + 2) * (1 - 2)"_str)) == -3); }

    SECTION("逻辑表达式组合") { REQUIRE((*vm.evalExpr<bool>("1 > 0 && 5 < 10 || 6 == 5"_str)) == true); }
}