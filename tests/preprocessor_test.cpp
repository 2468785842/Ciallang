//
// Created by LiDong on 2026/1/14.
//

#include <catch.hpp>

#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

TEST_CASE("preprocessor basic @if/@endif") {
    Runtime rt{};
    Context context{ rt };
    context.pp().setVar("version"_str, 0x02040009);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        @if (version >= 0x02040009)
            var a = "version >= 0x02040009";
        @endif
    )"_str);

    REQUIRE(**vm.evalExpr<String *>("a"_str) == "version >= 0x02040009"_str);
}

TEST_CASE("preprocessor @set and variable usage") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        @set(flag = 1)
        @if(flag)
            var b = "flag is set";
        @endif
    )"_str);

    REQUIRE(**vm.evalExpr<String *>("b"_str) == "flag is set"_str);
}

TEST_CASE("preprocessor nested @if") {
    Runtime rt{};
    Context context{ rt };
    context.pp().setVar("version"_str, 0x02040009);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        var c;
        @if(version >= 0x02040009)
            @if(0)
                c = "should not appear";
            @endif
            var d = "nested if works";
        @endif
    )"_str);

    REQUIRE(vm.evalExpr<Value>("c"_str)->isVoid()); // 不应该被定义
    REQUIRE(**vm.evalExpr<String *>("d"_str) == "nested if works"_str);
}

TEST_CASE("preprocessor binary/hex literals") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        @set(x = 0x10)
        @set(y = 0b1010)
        @if(x == 16 && y == 10)
            var e = "literals ok";
        @endif
    )"_str);

    REQUIRE(**vm.evalExpr<String *>("e"_str) == "literals ok"_str);
}

TEST_CASE("preprocessor disabled code via @if") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        @set(flag = 0)
        var f;
        @if(flag)
            f = "should not appear";
        @endif
    )"_str);

    REQUIRE(vm.evalExpr<Value>("f"_str)->isVoid()); // flag == 0, f 不定义
}

TEST_CASE("preprocessor comments ignored") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        // @set(flag=1)
        /* @set(x=42) */
        var g;
        @set(flag=0)
        @if(flag)
            g = "should not appear";
        @endif
        @set(flag=1)
        @if(flag)
            var h = "should appear";
        @endif
    )"_str);

    REQUIRE(vm.evalExpr<Value>("g"_str)->isVoid()); // 注释中的 @set 不生效
    REQUIRE(**vm.evalExpr<String *>("h"_str) == "should appear"_str);
}

TEST_CASE("preprocessor logical operators") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        @set(a = 1)
        @set(b = 0)
        @if(a && !b)
            var i = "logical && ! works";
        @endif
        @if(a || b)
            var j = "logical || works";
        @endif
        @if(a & 1)
            var k = "bitwise & works";
        @endif
        @if(a | 0)
            var l = "bitwise | works";
        @endif
        @if(a ^ 0)
            var m = "bitwise ^ works";
        @endif
    )"_str);

    REQUIRE(**vm.evalExpr<String *>("i"_str) == "logical && ! works"_str);
    REQUIRE(**vm.evalExpr<String *>("j"_str) == "logical || works"_str);
    REQUIRE(**vm.evalExpr<String *>("k"_str) == "bitwise & works"_str);
    REQUIRE(**vm.evalExpr<String *>("l"_str) == "bitwise | works"_str);
    REQUIRE(**vm.evalExpr<String *>("m"_str) == "bitwise ^ works"_str);
}

TEST_CASE("preprocessor complex nested expressions") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        @set(flag = 1)
        @set(version = 0x02040009)
        @if(version >= 0x02040009 && flag)
            var n = "complex nested works";
        @endif
    )"_str);

    REQUIRE(**vm.evalExpr<String *>("n"_str) == "complex nested works"_str);
}