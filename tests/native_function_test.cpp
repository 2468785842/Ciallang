/*
 * Copyright (c) 2026/1/4
 *
 * 本地函数单元测试
 */

#include <catch.hpp>

#include "stdlib/StringLib.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;
using namespace cial::stdlib;

TEST_CASE("本地函数声明 - 字符串成员方法定义") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    const VM vm{ &vmState };

    SECTION("Method - charAt") {
        REQUIRE(**vm.evalExpr<String *>(R"("123".charAt(2))"_str) == "3"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("cab".charAt(0))"_str) == "c"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("cab".charAt(3))"_str) == ""_str);
        REQUIRE(**vm.evalExpr<String *>(R"("".charAt(0))"_str) == ""_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abc".charAt(-1))"_str) == ""_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abc".charAt(100))"_str) == ""_str);
    }

    SECTION("Method - indexOf") {
        REQUIRE(*vm.evalExpr<Integer>(R"("hello".indexOf("e"))"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>(R"("hello".indexOf("l"))"_str) == 2);
        REQUIRE(*vm.evalExpr<Integer>(R"("hello".indexOf("x"))"_str) == -1);
        REQUIRE(*vm.evalExpr<Integer>(R"("hello".indexOf(""))"_str) == -1);
        REQUIRE(*vm.evalExpr<Integer>(R"("".indexOf("a"))"_str) == -1);
    }

    SECTION("Method - to(Upper|Lower)Case") {
        REQUIRE(**vm.eval<String *>(R"(var abc = "AbC"; abc.toUpperCase(); return abc;)"_str) == "ABC"_str);
        REQUIRE(**vm.eval<String *>(R"(var abc = "AbC"; abc.toLowerCase(); return abc;)"_str) == "abc"_str);
        REQUIRE(**vm.eval<String *>(R"(var empty = ""; empty.toUpperCase(); return empty;)"_str) == ""_str);
    }

    SECTION("Method - substring") {
        REQUIRE(**vm.evalExpr<String *>(R"("abcdef".substring(1, 4))"_str) == "bcde"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abcdef".substring(0, 6))"_str) == "abcdef"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abcdef".substring(3, 3))"_str) == "def"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abcdef".substring(4, 2))"_str) == "ef"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abcdef".substring(-1, 3))"_str) == ""_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abcdef".substring(2))"_str) == "cdef"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abcdef".substring(6, 2))"_str) == ""_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abcdef".substring(5, 10))"_str) == "f"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abcdef".substring(0, 0))"_str) == ""_str);
    }

    SECTION("Method - sprintf") {
        REQUIRE(**vm.evalExpr<String *>(R"("%d-%s".sprintf(10, "ok"))"_str) == "10-ok"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("%s".sprintf("test"))"_str) == "test"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("".sprintf())"_str) == ""_str);
    }

    // SECTION("Method - replace") {
    //     REQUIRE(**vm.evalExpr<String *>(R"("foo bar foo".replace("foo", "baz"))"_str) == "baz bar baz"_str);
    //     REQUIRE(**vm.evalExpr<String *>(R"("hello".replace("x", "y"))"_str) == "hello"_str);
    //     REQUIRE(**vm.evalExpr<String *>(R"("aaa".replace("a", ""))"_str) == ""_str);
    // }

    SECTION("Method - escape") {
        REQUIRE(**vm.evalExpr<String *>(R"("a\nb\tc".escape())"_str) == "a\\nb\\tc"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("".escape())"_str) == ""_str);
    }

    // SECTION("Method - split") {
    //     REQUIRE(vm.evalExpr<String *>(R"("a,b,c".split(","))"_str)->length() == 3);
    //     REQUIRE(vm.evalExpr<String *>(R"("a,,b".split(","))"_str)->length() == 3);
    //     REQUIRE(vm.evalExpr<String *>(R"("".split(","))"_str)->length() == 1);
    // }

    SECTION("Method - trim") {
        REQUIRE(**vm.evalExpr<String *>(R"("  hello  ".trim())"_str) == "hello"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("\n\t a \t".trim())"_str) == "a"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("".trim())"_str) == ""_str);
    }

    SECTION("Method - reverse") {
        REQUIRE(**vm.eval<String *>(R"(var str = "abc"; str.reverse(); return str;)"_str) == "cba"_str);
        REQUIRE(**vm.eval<String *>(R"(var str = "a"; str.reverse(); return str;)"_str) == "a"_str);
        REQUIRE(**vm.eval<String *>(R"(var str = ""; str.reverse(); return str;)"_str) == ""_str);
    }

    SECTION("Method - repeat") {
        REQUIRE(**vm.evalExpr<String *>(R"("ab".repeat(3))"_str) == "ababab"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("ab".repeat(0))"_str) == ""_str);
        REQUIRE(**vm.evalExpr<String *>(R"("ab".repeat(-1))"_str) == ""_str);
    }
}
TEST_CASE("本地函数声明 - 字符串 length 只读属性") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    const VM vm{ &vmState };

    SECTION("Property - length") {
        REQUIRE(*vm.evalExpr<Integer>(R"("hello".length)"_str) == 5);
        REQUIRE(*vm.evalExpr<Integer>(R"("".length)"_str) == 0);
    }
}

TEST_CASE("本地函数声明 - Array/Dictionary 内建方法") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("Array push/pop/unshift/shift") {
        vm.eval(R"(
            var a = [1, 2];
            var c1 = a.push(3, 4);
            var p = a.pop();
            var c2 = a.unshift(0);
            var s = a.shift();
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("c1"_str) == 4);
        REQUIRE(*vm.evalExpr<Integer>("p"_str) == 4);
        REQUIRE(*vm.evalExpr<Integer>("c2"_str) == 4);
        REQUIRE(*vm.evalExpr<Integer>("s"_str) == 0);
    }

    SECTION("Array insert/remove/erase/join/reverse") {
        vm.eval(R"(
            var a = [1, 2, 3];
            a.insert(1, 9);
            var j1 = a.join("-");
            a.remove(1);
            a.reverse();
            var j2 = a.join(",");
            a.erase();
            var c = a.count;
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("j1"_str)) == "1-9-2-3"_str);
        REQUIRE((**vm.evalExpr<String *>("j2"_str)) == "3,2,1"_str);
        REQUIRE(*vm.evalExpr<Integer>("c"_str) == 0);
    }

    SECTION("Dictionary assign/clear/erase") {
        vm.eval(R"(
            var d1 = %[a : 1, b : 2];
            var d2 = %[];
            (Dictionary.assign incontextof d2)(d1);
            var c1 = d2.count;
            var erased = d2.erase("a");
            var c2 = d2.count;
            d2.clear();
            var c3 = d2.count;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("c1"_str) == 2);
        REQUIRE(*vm.evalExpr<Integer>("erased"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("c2"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("c3"_str) == 0);
    }

    SECTION("count 属性可写（调整长度）") {
        vm.eval(R"(
            var a = [1, 2, 3];
            a.count = 5;
            var c1 = a.count;
            a.length = 1;
            var c2 = a.count;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("c1"_str) == 5);
        REQUIRE(*vm.evalExpr<Integer>("c2"_str) == 1);
    }
}
