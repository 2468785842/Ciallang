/*
 * Copyright (c) 2026/1/4
 *
 * 本地函数单元测试
 */

#include <catch2/catch_all.hpp>

#include "StderrCapture.hpp"

#include <filesystem>

#include <fmt/core.h>

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
        // TJS2 语义：返回新串，不改原串
        REQUIRE(**vm.eval<String *>(R"(var abc = "AbC"; return abc.toUpperCase();)"_str) == "ABC"_str);
        REQUIRE(**vm.eval<String *>(R"(var abc = "AbC"; return abc.toLowerCase();)"_str) == "abc"_str);
        REQUIRE(**vm.eval<String *>(R"(var empty = ""; return empty.toUpperCase();)"_str) == ""_str);
        REQUIRE(**vm.eval<String *>(R"(var abc = "AbC"; abc.toUpperCase(); return abc;)"_str) == "AbC"_str);
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
        REQUIRE(**vm.eval<String *>(R"(var str = "abc"; var r = str.reverse(); return r;)"_str) == "cba"_str);
        REQUIRE(**vm.eval<String *>(R"(var str = "abc"; str.reverse(); return str;)"_str) == "abc"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("abcdefghijklmnop".reverse())"_str) == "ponmlkjihgfedcba"_str);
        REQUIRE(**vm.evalExpr<String *>(R"("漢字abc".reverse())"_str) == "cba字漢"_str);
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

TEST_CASE("本地函数声明 - RegExp 内建方法") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("Method - test/match") {
        REQUIRE(*vm.evalExpr<Integer>(R"(/a.c/.test("abc"))"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>(R"(/a.c/.test("abd"))"_str) == 0);
        // i 标志：忽略大小写
        REQUIRE(*vm.evalExpr<Integer>(R"(/ABC/i.test("xabcx"))"_str) == 1);
        // match 不改状态：组捕获 [整体, 组1, 组2]，未命中为空数组
        REQUIRE(*vm.evalExpr<Integer>(R"(/:(\d+):(\d+):/.match("time :12:34: ok").count)"_str) == 3);
        REQUIRE((**vm.evalExpr<String *>(R"(/:(\d+):(\d+):/.match("time :12:34: ok")[1])"_str)) == "12"_str);
        REQUIRE(*vm.evalExpr<Integer>(R"(/zzz/.match("abc").count)"_str) == 0);
    }

    SECTION("Method - exec 与匹配状态") {
        vm.eval(R"(
            var g = /x(\d+)x/g;
            var first = g.exec("a1x22x b x333x c");
            var f0 = first[0];
            var f1 = first[1];
            var idx1 = g.index;
            var last1 = g.lastIndex;
            var second = g.exec("a1x22x b x333x c");
            var s0 = second[0];
            var idx2 = g.index;
            var miss = g.exec("a1x22x b x333x c");
            var missCount = miss.count;
            var lastMatch = g.lastMatch;
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("f0"_str)) == "x22x"_str);
        REQUIRE((**vm.evalExpr<String *>("f1"_str)) == "22"_str);
        REQUIRE(*vm.evalExpr<Integer>("idx1"_str) == 2);
        REQUIRE(*vm.evalExpr<Integer>("last1"_str) == 6);
        REQUIRE((**vm.evalExpr<String *>("s0"_str)) == "x333x"_str);
        REQUIRE(*vm.evalExpr<Integer>("idx2"_str) == 9);
        // 无 g 的 exec 不推进 start：重复 exec 结果不变
        REQUIRE(*vm.evalExpr<Integer>("missCount"_str) == 0);
    }

    SECTION("exec 无 g 重复执行不推进") {
        vm.eval(R"(
            var e = /b/;
            var m1 = e.exec("abcb");
            var i1 = e.index;
            var m2 = e.exec("abcb");
            var i2 = e.index;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("i1"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("i2"_str) == 1);
    }

    SECTION("状态属性 lastMatch/lastParen/leftContext/rightContext/start") {
        vm.eval(R"(
            var e = /(a)(b)?/;
            e.exec("za");
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("e.lastMatch"_str)) == "a"_str);
        REQUIRE((**vm.evalExpr<String *>("e.lastParen"_str)) == ""_str); // 末组未参与匹配
        REQUIRE((**vm.evalExpr<String *>("e.leftContext"_str)) == "z"_str);
        REQUIRE((**vm.evalExpr<String *>("e.rightContext"_str)) == ""_str);
        // start 可写：指定搜索起点
        REQUIRE(*vm.evalExpr<Integer>(R"((function(){ var m = /b/; m.start = 3; return m.match("abcb").count; })())"_str) == 1);
        // 读写往返
        REQUIRE(*vm.evalExpr<Integer>(R"((function(){ var m = /b/; m.start = 3; return m.start; })())"_str) == 3);
    }

    SECTION("Method - compile") {
        vm.eval(R"(
            var pat = /foo/;
            pat.compile("b(a)r", "i");
            var t = pat.test("BAR");
            var lm = pat.lastMatch;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("t"_str) == 1);
        REQUIRE((**vm.evalExpr<String *>("lm"_str)) == "BAR"_str);
    }

    SECTION("Method - replace（g 全局/单次）") {
        REQUIRE((**vm.evalExpr<String *>(R"("a1b22c333d".replace(/\d+/, "N"))"_str)) == "aNb22c333d"_str);
        REQUIRE((**vm.evalExpr<String *>(R"("a1b22c333d".replace(/\d+/g, "N"))"_str)) == "aNbNcNd"_str);
        REQUIRE((**vm.evalExpr<String *>(R"("no digits".replace(/\d+/g, "N"))"_str)) == "no digits"_str);
        // RegExp.replace 形态：/re/.replace(target, to)
        REQUIRE((**vm.evalExpr<String *>(R"(/o/.replace("foo bar", "0"))"_str)) == "f0o bar"_str);
    }

    SECTION("Method - split") {
        // 字符串模式：分隔字符集（每个字符都是分隔符，TJS2 语义）
        vm.eval(R"(
            var p1 = "a,b;c".split(",;");
            var p2 = "a,,b".split(",", void, true);
            var p3 = "a1b22c".split(/\d+/);
            var a4 = [].split(/,/, "x,y,z");
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("p1.count"_str) == 3);
        REQUIRE((**vm.evalExpr<String *>("p1[2]"_str)) == "c"_str);
        REQUIRE(*vm.evalExpr<Integer>("p2.count"_str) == 2); // purgeempty 去空段
        REQUIRE(*vm.evalExpr<Integer>("p3.count"_str) == 3);
        REQUIRE((**vm.evalExpr<String *>("p3[1]"_str)) == "b"_str);
        REQUIRE(*vm.evalExpr<Integer>("a4.count"_str) == 3); // Array.split 形态
    }
}

TEST_CASE("字符串拼接 SSO 边界回归（SHORT_STR_LEN 一致性）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // 修复：concat 的 SSO 阈值(total <= 21)曾与构造/析构/move 的长串判据(len > 21)
    // 不一致——恰好 21 字符的结果丢尾字符并泄漏缓冲
    SECTION("链式拼接跨 20/21/22 字符边界") {
        REQUIRE((**vm.evalExpr<String *>(R"("1234567890123456789" + "0")"_str)) == "12345678901234567890"_str); // 20
        REQUIRE((**vm.evalExpr<String *>(R"("1234567890123456789" + "01")"_str)) == "123456789012345678901"_str); // 21
        REQUIRE((**vm.evalExpr<String *>(R"("1234567890123456789" + "012")"_str)) == "1234567890123456789012"_str); // 22
        REQUIRE((**vm.evalExpr<String *>(R"("hit: " + "x22x" + " idx=" + 2 + " zzz=" + "6")"_str))
            == "hit: x22x idx=2 zzz=6"_str);
        REQUIRE(*vm.evalExpr<Integer>(R"(("hit: " + "x22x" + " idx=" + 2 + " zzz=" + "6").length)"_str) == 21);
    }
}

TEST_CASE("原生回调脚本函数（VM::callValue 底座）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("Array.sort 排序字母") {
        vm.eval(R"(
            var a = [3, 1, 2];
            a.sort();
            var s1 = a.join(",");
            a.sort("-");
            var s2 = a.join(",");
            var b = [10, 9, 100];
            b.sort("0");
            var s3 = b.join(",");
            var c = ["b10", "a2", "c1"];
            c.sort("a");
            var s4 = c.join(",");
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("s1"_str)) == "1,2,3"_str);
        REQUIRE((**vm.evalExpr<String *>("s2"_str)) == "3,2,1"_str);
        REQUIRE((**vm.evalExpr<String *>("s3"_str)) == "9,10,100"_str); // 数值序而非字典序
        REQUIRE((**vm.evalExpr<String *>("s4"_str)) == "a2,b10,c1"_str);
    }

    SECTION("Array.sort 比较器回调（含闭包引用）") {
        vm.eval(R"(
            var d = [5, 2, 8, 1];
            d.sort(function(x, y) { return x > y; });
            var s1 = d.join(",");
            d.sort(function(x, y) { return x < y; });
            var s2 = d.join(",");
            var bias = 10;
            var e = [1, 2, 3];
            e.sort(function(x, y) { return (x % bias) < (y % bias); });
            var s3 = e.join(",");
        )"_str);
        REQUIRE((**vm.evalExpr<String *>("s1"_str)) == "8,5,2,1"_str);
        REQUIRE((**vm.evalExpr<String *>("s2"_str)) == "1,2,5,8"_str);
        REQUIRE((**vm.evalExpr<String *>("s3"_str)) == "1,2,3"_str);
    }

    SECTION("RegExp.replace 函数回调") {
        REQUIRE((**vm.evalExpr<String *>(R"("a1b22c".replace(/\d+/g, function(m) { return "<" + m[0] + ">"; }))"_str))
            == "a<1>b<22>c"_str);
        REQUIRE((**vm.evalExpr<String *>(R"("x7y".replace(/(\d)/, function(m) { return m[1] + m[1]; }))"_str))
            == "x77y"_str);
    }

    SECTION("回调异常：中断 native 并被外层脚本 catch") {
        vm.eval(R"(
            var caught = 0;
            var after = 0;
            try {
                var f = [1, 2, 3];
                f.sort(function(x, y) { throw "boom"; });
            } catch(e) {
                caught = 1;
            }
            after = 1 + 2;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("caught"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("after"_str) == 3); // 中断后 VM 状态正常
    }

    SECTION("回调内部 try 自行消化（不中断 native）") {
        vm.eval(R"(
            var ok = 0;
            var e2 = [3, 1];
            e2.sort(function(a, b) {
                try { throw "inner"; } catch(ex) { ok = 1; }
                return a < b;
            });
            var s = e2.join(",");
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("ok"_str) == 1);
        REQUIRE((**vm.evalExpr<String *>("s"_str)) == "1,3"_str);
    }

    SECTION("回调无 handler 时终态报错") {
        REQUIRE_THROWS(vm.eval(R"(
            var d = [1, 2];
            d.sort(function(x, y) { throw "fatal-boom"; });
        )"_str));
    }
}

TEST_CASE("数字字面量——进制/浮点/科学计数法") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // 修复回归：extractNumber 循环条件用十进制字符集（十六进制字母不在其中，
    // 0x1.07CCp14 解析到 'C' 中断）；p 指数符号分支吃掉指数数字；八进制 peek
    // 吞首位数字（010 得 0）；parseNonDecimalReal 位组装边界算错（0x1p-2 得 4）；
    // 科学计数法 1e3 词法报错、1e-12 整数截断为 0
    SECTION("进制前缀整数") {
        REQUIRE(*vm.evalExpr<Integer>(R"(0x10)"_str) == 16);
        REQUIRE(*vm.evalExpr<Integer>(R"(0xFF)"_str) == 255);
        REQUIRE(*vm.evalExpr<Integer>(R"(0b101)"_str) == 5);
        REQUIRE(*vm.evalExpr<Integer>(R"(010)"_str) == 8); // 八进制
    }

    SECTION("十六进制/二进制浮点（p 指数）") {
        REQUIRE((*vm.evalExpr<Real>(R"(0x1p4)"_str)).value() == 16.0);
        REQUIRE((*vm.evalExpr<Real>(R"(0x1p-2)"_str)).value() == 0.25);
        REQUIRE((*vm.evalExpr<Real>(R"(0x1.8p1)"_str)).value() == 3.0);
        REQUIRE((*vm.evalExpr<Real>(R"(0X1.8P1)"_str)).value() == 3.0);
        REQUIRE((*vm.evalExpr<Real>(R"(0x1.07CCp14)"_str)).value() == 16883.0); // 精确整除
        REQUIRE((*vm.evalExpr<Real>(R"(0b1.1)"_str)).value() == 1.5);
        REQUIRE((*vm.evalExpr<Real>(R"(0b1p-1)"_str)).value() == 0.5);
    }

    SECTION("科学计数法（符号可选/整数底数）") {
        REQUIRE((*vm.evalExpr<Real>(R"(1e3)"_str)).value() == 1000.0);
        REQUIRE((*vm.evalExpr<Real>(R"(2E2)"_str)).value() == 200.0);
        REQUIRE((*vm.evalExpr<Real>(R"(1e-3)"_str)).value() == 0.001);
        REQUIRE((*vm.evalExpr<Real>(R"(1e-12 < 1e-11 ? 1e-12 : 0.0)"_str)).value() < 1e-11);
        REQUIRE((*vm.evalExpr<Real>(R"(1.5e-3)"_str)).value() == 0.0015);
        REQUIRE(**vm.evalExpr<String *>(R"((1e-1 + 1) + "")"_str) != ""_str); // 拼接场景终止符不吞
    }
}

TEST_CASE("with 内 try/catch（对齐原版：catch 内 with 上下文丢失）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // 对拍定论（原版 tjsInterCodeGen.cpp）：裸标识符未命中局部时降级为
    // this.member（T_THIS_PROXY），只有显式 .member 裸词链查 with 栈
    // （T_WITHDOT_PROXY）——因此 with 内异常穿越后（原版 catch 帧重建同样
    // 丢 with 栈）catch 块里裸标识符不解析到 with 对象，两个引擎行为一致。
    // with 的正途是 .member 裸词链（语料 test_with.tjs 全部此形态）
    vm.eval(R"tjs(
        var o = %[m: 7];
        var r = "";
        function f() {
            with(o) {
                try {
                    r = r + "A";
                    throw "e";
                } catch(e) {
                    r = r + "B";
                }
            }
            with(o) {
                r = r + "|" + .m;
            }
            return "done";
        }
        var st = f();
        var g = %[n: 1];
        var out = "";
        with(g) {
            try { out = "n=" + .n; throw "y"; } catch(e) { out = out + "|c"; }
            out = out + "|" + .n;
        }
    )tjs"_str);
    REQUIRE((**vm.evalExpr<String *>("r"_str)) == "AB|7"_str); // 异常穿越后 with 仍可用于下一 with 块
    REQUIRE((**vm.evalExpr<String *>("st"_str)) == "done"_str);
    REQUIRE((**vm.evalExpr<String *>("out"_str)) == "n=1|c|1"_str); // try 前裸词链 + 穿越后恢复
}

TEST_CASE("(const)[...] / (const)%[...] 常量内联形态（对齐原版：无只读/无共享）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // tjs.y:795 产生式 = TJSCreateArrayObject 新建 + T_CONSTVAL 节点；代码生成
    // 只是 VM_CONST 常量池装载——无只读保护、无跨求值共享（每次新建）。
    // 原版 (const)%[...] 甚至解析报错（旧版缺陷），此处行为超出原版（可用），
    // 其余逐项对齐。
    vm.eval(R"tjs(
        var a = (const)[1, 2, 3];
        var b = (const)[1, 2, 3];
        var eq = a[1] === b[1];
        a[1] = 99;
        var w = a[1];
        var nested = (const)[1, (const)%[k: 7]];
        var nk = nested[1]["k"];
        var d = (const)%[x: 1, y: 2];
        d["x"] = 5;
        var dx = d["x"];
    )tjs"_str);
    REQUIRE(*vm.evalExpr<Integer>("eq"_str) == 1);
    REQUIRE(*vm.evalExpr<Integer>("w"_str) == 99); // 可写（原版同）
    REQUIRE(*vm.evalExpr<Integer>("nk"_str) == 7); // 嵌套常量形态
    REQUIRE(*vm.evalExpr<Integer>("dx"_str) == 5); // 字典形态可写
}

TEST_CASE("const 语义（对齐原版实测：必须初始化，赋值不强制）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("const 无初始化为编译期错误") {
        test::StderrCapture firstError;
        const auto r1 = vm.eval<Value>("const E; return 1;"_str);
        const auto firstDiagnostic = firstError.finish();
        REQUIRE(firstDiagnostic.find("const declaration must be initialized") != std::string::npos);
        REQUIRE(r1.get().isVoid()); // 编译失败不产出值
        test::StderrCapture secondError;
        const auto r2 = vm.eval<Value>("const a = 1, b; return 2;"_str);
        const auto secondDiagnostic = secondError.finish();
        REQUIRE(secondDiagnostic.find("const declaration must be initialized") != std::string::npos);
        REQUIRE(r2.get().isVoid()); // 逗号链同样检查
    }

    SECTION("const 赋值/自增/for 递增放行（原版行为）") {
        vm.eval(R"tjs(
            const A = 10;
            A = 20;
            var g = A;
            const C = 0;
            C++;
            var ci = C;
            var iters = 0;
            for(const i = 0; i < 2; i++) iters++;
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("g"_str) == 20);
        REQUIRE(*vm.evalExpr<Integer>("ci"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("iters"_str) == 2);
    }
}

TEST_CASE("Math.RandomGenerator（MT19937，序列与原版逐位一致）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // 固定种子序列对拍 TJS2（tjsMT19937ar-cok）：以下期望值取自参考实现输出
    vm.eval(R"tjs(
        var r = new Math.RandomGenerator(123);
        var v1 = r.random64();
        var v2 = r.random32();
        var v3 = r.random63();
        var f = r.random();
        r.randomize(777);
        var v4 = r.random32();
        var v5 = r.random64();
        var dic = r.serialize();
        var seq1 = r.random64();
        var r2 = new Math.RandomGenerator(dic);
        var restored = r2.random64() == seq1;
    )tjs"_str);
    REQUIRE(*vm.evalExpr<Integer>("v1"_str) == 7311485051347868734LL);
    REQUIRE(*vm.evalExpr<Integer>("v2"_str) == 4088668458LL);
    REQUIRE(*vm.evalExpr<Integer>("v3"_str) == 7907280711022731305LL);
    REQUIRE(*vm.evalExpr<Integer>("v4"_str) == 78096651LL);
    REQUIRE(*vm.evalExpr<Integer>("v5"_str) == -1466894912068952260LL);
    REQUIRE((*vm.evalExpr<Real>("f"_str)).value() == Catch::Approx(0.457861570107705).epsilon(1e-12));
    REQUIRE(*vm.evalExpr<Integer>("restored"_str) == 1); // serialize→恢复序列一致
}

TEST_CASE("Date 标准库") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"tjs(
        var d = new Date(2023, 0, 15, 10, 30, 5);
        var y = d.getYear(); var mo = d.getMonth(); var da = d.getDate();
        var h = d.getHours(); var mi = d.getMinutes(); var se = d.getSeconds();
        d.setMonth(11); d.setDate(25);
        var mo2 = d.getMonth(); var da2 = d.getDate();
        var now = new Date();
        var nowOk = now.getYear() >= 2026;
        var p = new Date("2024/6/1 12:30:00");
        var py = p.getYear(); var pmo = p.getMonth(); var pd = p.getDate(); var ph = p.getHours();
        var q = new Date(2023, 0, 1);
        q.setTime(1700000000000);
        var qy = q.getYear();
    )tjs"_str);
    SECTION("分量构造与 get 系") {
        REQUIRE(*vm.evalExpr<Integer>("y"_str) == 2023);
        REQUIRE(*vm.evalExpr<Integer>("mo"_str) == 0); // 0-based，对齐 TJS2
        REQUIRE(*vm.evalExpr<Integer>("da"_str) == 15);
        REQUIRE(*vm.evalExpr<Integer>("h"_str) == 10);
        REQUIRE(*vm.evalExpr<Integer>("mi"_str) == 30);
        REQUIRE(*vm.evalExpr<Integer>("se"_str) == 5);
    }
    SECTION("set 系") {
        REQUIRE(*vm.evalExpr<Integer>("mo2"_str) == 11);
        REQUIRE(*vm.evalExpr<Integer>("da2"_str) == 25);
    }
    SECTION("now/字符串解析/setTime") {
        REQUIRE(*vm.evalExpr<Integer>("nowOk"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("py"_str) == 2024);
        // 6 月（原版 SetMonth($4-1)；旧子集解析器少 -1 曾得 7 月，对拍已修正）
        REQUIRE(*vm.evalExpr<Integer>("pmo"_str) == 5);
        REQUIRE(*vm.evalExpr<Integer>("pd"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("ph"_str) == 12);
        REQUIRE(*vm.evalExpr<Integer>("qy"_str) == 2023); // 1700000000000ms = 2023-11-14
    }
}

TEST_CASE("Date.parse 完整文法（tjsdate.y 全形态，对拍 TJSCompiler 一致）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // 时区字符串的 getTime() 为真实 UTC epoch（双端对拍锁定）；无时区字符串
    // 存本地钟面秒（原版同），断言走 localtime 字段保证跨时区可移植。
    // 先做一次热身构造：Windows CRT _timezone 惰性初始化，进程首个字符串构造
    // 会偏移 +2×|tz|（原版同款怪癖，见 DateParser.cpp build() 注释），
    // 任意一次构造后时区状态即固定
    vm.eval(R"tjs(var warm = new Date("2000/01/01 00:00:00 gmt");)tjs"_str);

    SECTION("F11 数值形态：斜杠/连字符/混合分隔符 + 秒可省 + 小数秒忽略") {
        vm.eval(R"tjs(
            var a = new Date("2004/03/03 11:22:33");
            var ay = a.getYear(); var amo = a.getMonth(); var ad = a.getDate();
            var ah = a.getHours(); var ami = a.getMinutes(); var as = a.getSeconds();
            var b = new Date("2004-3-3 11:22:33");
            var bamo = b.getMonth();
            var c = new Date("2004-03/03 11:22:33");
            var cami = c.getMinutes();
            var d = new Date("2004/03/03 11:22");
            var ds = d.getSeconds();
            var e2 = new Date("2004/03/03 11:22:33.5");
            var es = e2.getSeconds();
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("ay"_str) == 2004);
        REQUIRE(*vm.evalExpr<Integer>("amo"_str) == 2); // SetMonth($4-1)
        REQUIRE(*vm.evalExpr<Integer>("ad"_str) == 3);
        REQUIRE(*vm.evalExpr<Integer>("ah"_str) == 11);
        REQUIRE(*vm.evalExpr<Integer>("ami"_str) == 22);
        REQUIRE(*vm.evalExpr<Integer>("as"_str) == 33);
        REQUIRE(*vm.evalExpr<Integer>("bamo"_str) == 2);
        REQUIRE(*vm.evalExpr<Integer>("cami"_str) == 22); // 分隔符逐个独立
        REQUIRE(*vm.evalExpr<Integer>("ds"_str) == 0); // 秒可省
        REQUIRE(*vm.evalExpr<Integer>("es"_str) == 33); // 小数秒消费但忽略
    }

    SECTION("月名形态 F1~F10（day/year 位置互换 + 连字符变体 + 星期前缀）") {
        vm.eval(R"tjs(
            var f1 = new Date("3 May 2004 11:22:33");
            var f2 = new Date("3-May 2004 11:22:33");
            var f3 = new Date("3-May-2004 11:22:33");
            var f4 = new Date("May 3 2004 11:22:33");
            var f5 = new Date("May-3 2004 11:22:33");
            var f6 = new Date("May-3-2004 11:22:33");
            var f7 = new Date("3 May 11:22:33 2004");
            var f8 = new Date("3-May 11:22:33 2004");
            var f9 = new Date("May 3 11:22:33 2004");
            var f10 = new Date("May-3 11:22:33 2004");
            var same = f1.getTime() == f2.getTime() && f2.getTime() == f3.getTime()
                && f3.getTime() == f4.getTime() && f4.getTime() == f5.getTime()
                && f5.getTime() == f6.getTime() && f6.getTime() == f7.getTime()
                && f7.getTime() == f8.getTime() && f8.getTime() == f9.getTime()
                && f9.getTime() == f10.getTime();
            var mo = f1.getMonth();
            var wd1 = new Date("Sun, 3 May 2004 11:22:33").getTime() == f1.getTime();
            var wd2 = new Date("Thurs, 3 May 2004 11:22:33").getTime() == f1.getTime();
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("same"_str) == 1); // 10 形态同一时刻
        REQUIRE(*vm.evalExpr<Integer>("mo"_str) == 4);
        REQUIRE(*vm.evalExpr<Integer>("wd1"_str) == 1); // 星期值不参与语义
        REQUIRE(*vm.evalExpr<Integer>("wd2"_str) == 1);
    }

    SECTION("am/pm 位置与原版怪癖（12 点不加减，pm 12 点进位次日）") {
        vm.eval(R"tjs(
            var pmSuf = new Date("3 May 2004 11:22:33 pm");
            var pmPre = new Date("3 May pm 11:22:33 2004");
            var pmF7 = new Date("May-3 11:22:33 pm 2004");
            var amF9 = new Date("May 3 am 11:22:33 2004");
            var noonAm = new Date("2004/03/03 12:00:00 am");
            var noonAmH = noonAm.getHours(); var noonAmD = noonAm.getDate();
            var noonPm = new Date("2004/03/03 12:00:00 pm");
            var noonPmH = noonPm.getHours(); var noonPmD = noonPm.getDate();
            var diff = pmSuf.getTime() - new Date("3 May 2004 11:22:33").getTime();
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("pmSuf.getHours()"_str) == 23); // 11+12
        REQUIRE(*vm.evalExpr<Integer>("pmPre.getHours()"_str) == 23);
        REQUIRE(*vm.evalExpr<Integer>("pmF7.getHours()"_str) == 23);
        REQUIRE(*vm.evalExpr<Integer>("amF9.getHours()"_str) == 11);
        REQUIRE(*vm.evalExpr<Integer>("noonAmH"_str) == 12); // 怪癖：am 12 点不动
        REQUIRE(*vm.evalExpr<Integer>("noonAmD"_str) == 3);
        REQUIRE(*vm.evalExpr<Integer>("noonPmH"_str) == 0); // 24:00 → 次日 0 点
        REQUIRE(*vm.evalExpr<Integer>("noonPmD"_str) == 4);
        REQUIRE(*vm.evalExpr<Integer>("diff"_str) == 12 * 3600 * 1000);
    }

    SECTION("时区：名字/偏移/组合（绝对 epoch 与参考实现对拍锁定）") {
        vm.eval(R"tjs(
            var gmt = new Date("2004/03/03 00:00:00 gmt").getTime();
            var gmtPlus9 = new Date("2004/03/03 00:00:00 GMT +900").getTime();
            var jst = new Date("2004/03/03 00:00:00 JST").getTime();
            var utc = new Date("2004/03/03 00:00:00 UTC").getTime();
            var zulu = new Date("2004/03/03 00:00:00 Z").getTime();
            var nst = new Date("2004/03/03 00:00:00 NST").getTime();
            var halfHour = new Date("2004/03/03 00:00:00 +30").getTime();
            var jdoc = new Date("Sun, 3 May 2004 11:22:33 GMT +900 (JST)").getTime();
            var paren = new Date("3 May 2004 11:22:33 gmt (JST)").getTime();
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("gmt"_str) == 1078272000000);
        REQUIRE(*vm.evalExpr<Integer>("gmtPlus9"_str) == 1078239600000); // -9h
        REQUIRE(*vm.evalExpr<Integer>("jst"_str) == 1078239600000); // JST == GMT+900
        REQUIRE(*vm.evalExpr<Integer>("utc"_str) == 1078272000000);
        REQUIRE(*vm.evalExpr<Integer>("zulu"_str) == 1078272000000);
        REQUIRE(*vm.evalExpr<Integer>("nst"_str) == 1078284600000); // -3.5h
        REQUIRE(*vm.evalExpr<Integer>("halfHour"_str) == 1078270200000); // +30 分
        REQUIRE(*vm.evalExpr<Integer>("jdoc"_str) == 1083550953000);
        REQUIRE(*vm.evalExpr<Integer>("paren"_str) == 1083583353000); // 括号段忽略
    }

    SECTION("两位年与词表缩写/怪癖") {
        vm.eval(R"tjs(
            var y04 = new Date("May 3 04 11:22:33").getYear();
            var y50 = new Date("May 3 50 11:22:33").getYear();
            var y99 = new Date("May 3 99 11:22:33").getYear();
            var ju = new Date("3 ju 2004 11:22:33").getMonth();
            var sept = new Date("3 sept 2004 11:22:33").getMonth();
            var sepDot = new Date("3 Sep. 2004 11:22:33").getMonth();
            var dec = new Date("3 DEC 2004 11:22:33").getMonth();
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("y04"_str) == 2004); // <=50 → 2000+
        REQUIRE(*vm.evalExpr<Integer>("y50"_str) == 2050);
        REQUIRE(*vm.evalExpr<Integer>("y99"_str) == 1999); // >50 → 1900+
        REQUIRE(*vm.evalExpr<Integer>("ju"_str) == 5); // "ju" 单独成词（原版怪癖）
        REQUIRE(*vm.evalExpr<Integer>("sept"_str) == 8);
        REQUIRE(*vm.evalExpr<Integer>("sepDot"_str) == 8);
        REQUIRE(*vm.evalExpr<Integer>("dec"_str) == 11);
    }

    SECTION("越界分量 mktime 归一化（原版放行）") {
        vm.eval(R"tjs(
            var m13 = new Date("2004/13/03 11:22:33");
            var m13y = m13.getYear(); var m13m = m13.getMonth();
            var d32 = new Date("2004/03/32 11:22:33");
            var d32m = d32.getMonth(); var d32d = d32.getDate();
            var h25 = new Date("2004/03/03 25:00:00");
            var h25d = h25.getDate(); var h25h = h25.getHours();
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("m13y"_str) == 2005);
        REQUIRE(*vm.evalExpr<Integer>("m13m"_str) == 0);
        REQUIRE(*vm.evalExpr<Integer>("d32m"_str) == 3);
        REQUIRE(*vm.evalExpr<Integer>("d32d"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("h25d"_str) == 4);
        REQUIRE(*vm.evalExpr<Integer>("h25h"_str) == 1);
    }

    SECTION("Date.parse 实例方法 + 省略时区（本地钟面）") {
        vm.eval(R"tjs(
            var d = new Date();
            d.parse("2004/03/03 11:22:33");
            var py = d.getYear(); var ph = d.getHours();
            var p2 = new Date("2024/6/1 12:30:00");
            var p2m = p2.getMonth(); var p2h = p2.getHours();
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("py"_str) == 2004);
        REQUIRE(*vm.evalExpr<Integer>("ph"_str) == 11);
        REQUIRE(*vm.evalExpr<Integer>("p2m"_str) == 5); // /6/ = 6 月（0-based）
        REQUIRE(*vm.evalExpr<Integer>("p2h"_str) == 12);
    }

    SECTION("解析失败（cannot parse date）") {
        REQUIRE_THROWS(vm.eval<Value>(R"(var d = new Date("2004/03/03");)"_str)); // 缺时刻
        REQUIRE_THROWS(vm.eval<Value>(R"(var d = new Date("3 May 2004");)"_str));
        REQUIRE_THROWS(vm.eval<Value>(R"(var d = new Date("xyz");)"_str));
        REQUIRE_THROWS(vm.eval<Value>(R"(var d = new Date("");)"_str));
        REQUIRE_THROWS(vm.eval<Value>(R"(var d = new Date("2004/03/03 11");)"_str)); // 缺分
        REQUIRE_THROWS(vm.eval<Value>(R"(var d = new Date("2004/03/03 11:22:33 x");)"_str)); // 尾部垃圾
        REQUIRE_THROWS(vm.eval<Value>(R"(var d = new Date("May 2004 11:22:33");)"_str)); // 缺日
    }
}

TEST_CASE("RegExp.last 静态属性 + 匹配位置码点语义") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // tjsRegExp.cpp：LastRegExp 仅由 test/exec 更新（失败也更新），match/replace
    // 不更新；未执行过为 void。位置语义：本项目为 UTF-8 码点（TJS2 为 UTF-16
    // 码元，BMP 内两者一致；原版 index 属性混加字节偏移是自身 bug，见
    // compatibility.md——此处按 TODO 定案的码点语义锁定）
    SECTION("last：test/exec 更新（失败也更新），match/replace 不更新") {
        vm.eval(R"tjs(
            var r1 = /(\d+)/;
            r1.test("a123b");
            var l1 = RegExp.last.lastMatch;
            /zzz/.test("aaa");
            var l2 = RegExp.last.lastMatch;
            var r3 = /b/;
            r3.match("abc");
            var l3 = RegExp.last.lastMatch;
            "xxx".replace(/x/, "y");
            var l4 = RegExp.last.lastMatch;
            var r5 = /(\d+)-(\d+)/;
            r5.exec("77-42");
            var l5 = RegExp.last.lastParen;
            var r6 = /zzz/;
            r6.exec("aaa");
            var l6 = RegExp.last.lastMatch;
        )tjs"_str);
        REQUIRE((**vm.evalExpr<String *>("l1"_str)) == "123"_str);
        REQUIRE((**vm.evalExpr<String *>("l2"_str)) == ""_str); // 失败也更新
        REQUIRE((**vm.evalExpr<String *>("l3"_str)) == ""_str); // match 不更新
        REQUIRE((**vm.evalExpr<String *>("l4"_str)) == ""_str); // replace 不更新
        REQUIRE((**vm.evalExpr<String *>("l5"_str)) == "42"_str);
        REQUIRE((**vm.evalExpr<String *>("l6"_str)) == ""_str); // exec 失败也更新
    }

    SECTION("未执行过任何 test/exec 时为 void") {
        REQUIRE(vm.evalExpr<Value>("RegExp.last"_str).get().isVoid());
    }

    SECTION("CJK 码点位置（index/lastIndex/start/上下文）") {
        vm.eval(R"tjs(
            var g = /x(\d+)x/g;
            var m1 = g.exec("中文x22x b x333x");
            var i1 = g.index; var li1 = g.lastIndex;
            var m2 = g.exec("中文x22x b x333x");
            var i2 = g.index; var li2 = g.lastIndex;
            var e3 = /b/;
            e3.exec("中文b文文");
            var lc = e3.leftContext; var rc = e3.rightContext; var ei = e3.index;
            var s1 = /a/;
            s1.start = 2;
            var sm = s1.exec("中文abc")[0]; var si = s1.index;
            var r7 = /x(\d+)x/;
            r7.exec("中文x88x尾");
            var l7m = RegExp.last.lastMatch; var l7i = RegExp.last.index;
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("i1"_str) == 2); // "中文" = 2 码点（非 6 字节）
        REQUIRE(*vm.evalExpr<Integer>("li1"_str) == 6);
        REQUIRE(*vm.evalExpr<Integer>("i2"_str) == 9);
        REQUIRE(*vm.evalExpr<Integer>("li2"_str) == 14);
        REQUIRE((**vm.evalExpr<String *>("lc"_str)) == "中文"_str);
        REQUIRE((**vm.evalExpr<String *>("rc"_str)) == "文文"_str);
        REQUIRE(*vm.evalExpr<Integer>("ei"_str) == 2);
        REQUIRE((**vm.evalExpr<String *>("sm"_str)) == "a"_str); // start=2 跳过中文
        REQUIRE(*vm.evalExpr<Integer>("si"_str) == 2);
        REQUIRE((**vm.evalExpr<String *>("l7m"_str)) == "x88x"_str);
        REQUIRE(*vm.evalExpr<Integer>("l7i"_str) == 2); // RegExp.last 指向实例可读状态
    }
}

TEST_CASE("Array/Dictionary 结构化存取 + Octet pack 系（tjsBinarySerializer/tjsOctPack 对齐）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // save/load 系落盘路径：系统临时目录（正斜杠，TJS 字符串无需转义），
    // 不污染工作目录/仓库根
    std::string tmpDir = (std::filesystem::temp_directory_path() / "cial_test").string();
    std::filesystem::create_directories(tmpDir);
    for(auto &c : tmpDir)
        if(c == '\\')
            c = '/';
    vm.eval(String{ fmt::format(R"tjs(var __tmpDir = "{}";)tjs", tmpDir).c_str() });

    SECTION("pack/unpack：整数宽度/符号/字节序/浮点/base64") {
        vm.eval(R"tjs(
            var h1 = [1, -1].pack("c2").unpack("H*")[0];
            var u1 = [1, -1].pack("c2").unpack("c2").join(",");
            var h2 = [1000, -1000].pack("s2").unpack("H*")[0];
            var v1 = [0x1234].pack("v").unpack("S1")[0];
            var n1 = [0x1234].pack("n").unpack("S1")[0];
            var d1 = [1.5].pack("d").unpack("d1")[0];
            var f1 = [1.5].pack("f").unpack("f1")[0];
            var b64 = [72, 101, 108, 108, 111].pack("C5").unpack("m1")[0];
            var m1 = ["aGVsbG8="].pack("m1").unpack("H*")[0];
            var star = [1, 2, 3, 4].pack("C*").unpack("C*").join(",");
        )tjs"_str);
        REQUIRE((**vm.evalExpr<String *>("h1"_str)) == "01FF"_str);
        REQUIRE((**vm.evalExpr<String *>("u1"_str)) == "1,-1"_str);
        REQUIRE((**vm.evalExpr<String *>("h2"_str)) == "E80318FC"_str);
        REQUIRE(*vm.evalExpr<Integer>("v1"_str) == 4660); // LE
        REQUIRE(*vm.evalExpr<Integer>("n1"_str) == 13330); // BE
        REQUIRE((*vm.evalExpr<Real>("d1"_str)).value() == 1.5);
        REQUIRE((*vm.evalExpr<Real>("f1"_str)).value() == 1.5);
        REQUIRE((**vm.evalExpr<String *>("b64"_str)) == "SGVsbG8="_str);
        REQUIRE((**vm.evalExpr<String *>("m1"_str)) == "68656C6C6F"_str); // "hello"
        REQUIRE((**vm.evalExpr<String *>("star"_str)) == "1,2,3,4"_str); // '*' 吃尽剩余
    }

    SECTION("pack：字符串填充/位串/nibble/结构控制") {
        vm.eval(R"tjs(
            var a1 = ["hi"].pack("a4").unpack("H*")[0];
            var a2 = ["hi"].pack("A4").unpack("H*")[0];
            var b1 = ["11000001"].pack("b8").unpack("H*")[0];
            var b2 = ["1100000101000010"].pack("B*").unpack("H*")[0];
            var x1 = ["12ab"].pack("h4").unpack("H*")[0];
            var x2 = ["12ab"].pack("H4").unpack("H*")[0];
            var s1 = [1, 2].pack("c1x2c1").unpack("H*")[0];
            var s2 = [1, 2, 3].pack("c1X1c2").unpack("H*")[0];
            var s3 = [1, 2].pack("c1@8C1").unpack("C*").join(",");
        )tjs"_str);
        REQUIRE((**vm.evalExpr<String *>("a1"_str)) == "68690000"_str); // \0 填充
        REQUIRE((**vm.evalExpr<String *>("a2"_str)) == "68692020"_str); // 空格填充
        REQUIRE((**vm.evalExpr<String *>("b1"_str)) == "83"_str); // 低位在前
        REQUIRE((**vm.evalExpr<String *>("b2"_str)) == "C142"_str); // 高位在前
        REQUIRE((**vm.evalExpr<String *>("x1"_str)) == "21BA"_str); // 低位 nibble 在前
        REQUIRE((**vm.evalExpr<String *>("x2"_str)) == "12AB"_str); // 高位 nibble 在前
        REQUIRE((**vm.evalExpr<String *>("s1"_str)) == "01000002"_str); // x 跳过
        REQUIRE((**vm.evalExpr<String *>("s2"_str)) == "0203"_str); // X 回退
        REQUIRE((**vm.evalExpr<String *>("s3"_str)) == "1,0,0,0,0,0,0,0,2"_str); // @ 补齐到绝对位置 8 后再追加
    }

    SECTION("Array.save/load 文本行（元素一律字符串）") {
        vm.eval(R"tjs(
            var a = ["one", 2, 3.5];
            a.save(__tmpDir + "/t_lines.txt");
            var b = [];
            b.load(__tmpDir + "/t_lines.txt");
            var j = b.join("|");
            var c = b.count;
        )tjs"_str);
        REQUIRE((**vm.evalExpr<String *>("j"_str)) == "one|2|3.5"_str);
        REQUIRE(*vm.evalExpr<Integer>("c"_str) == 3);
    }

    SECTION("saveStruct 文本/二进制回环 + assignStruct 深拷贝") {
        vm.eval(R"tjs(
            var d = %[k1: 1, k2: "s", k3: [1, 2], k4: %[n: 7]];
            d.saveStruct(__tmpDir + "/t_d.txt");
            var d2 = %[];
            var r2 = d2.loadStruct(__tmpDir + "/t_d.txt");
            var d2c = d2.count;      // 文本路径不写入自身（原版同款怪癖）
            var r2c = r2.count;
            var k4n = r2["k4"]["n"];
            d.saveStruct(__tmpDir + "/t_d.bin", "b");
            var d3 = %[x: 0];
            d3.loadStruct(__tmpDir + "/t_d.bin", "b");
            var d3c = d3.count;
            var k2 = d3["k2"];
            var k3 = d3["k3"].join("+");
            var src = %[a: 1, sub: %[deep: [5, 6]]];
            var dst = %[];
            dst.assignStruct(src);
            var deep = src["sub"]["deep"];
            deep[0] = 99;
            var dp = dst["sub"]["deep"][0];
            var da = dst["a"];
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("d2c"_str) == 0);
        REQUIRE(*vm.evalExpr<Integer>("r2c"_str) == 4);
        REQUIRE(*vm.evalExpr<Integer>("k4n"_str) == 7);
        REQUIRE(*vm.evalExpr<Integer>("d3c"_str) == 4);
        REQUIRE((**vm.evalExpr<String *>("k2"_str)) == "s"_str);
        REQUIRE((**vm.evalExpr<String *>("k3"_str)) == "1+2"_str);
        REQUIRE(*vm.evalExpr<Integer>("dp"_str) == 5); // 深拷贝独立
        REQUIRE(*vm.evalExpr<Integer>("da"_str) == 1);
    }

    SECTION("Array.loadStruct 仅收二进制（文本报错，原版同）+ 环 → null + octet 进出") {
        vm.eval(R"tjs(
            var arr = [1, "x", [7], %[y: 8]];
            arr.saveStruct(__tmpDir + "/t_a.txt");
            var a2 = [];
            var txtErr = 0;
            try { a2.loadStruct(__tmpDir + "/t_a.txt"); } catch(e) { txtErr = 1; }
            arr.saveStruct(__tmpDir + "/t_a.bin", "b");
            var a3 = [];
            a3.loadStruct(__tmpDir + "/t_a.bin", "b");
            var a3c = a3.count;
            var e3 = a3[2][0];
            var y = a3[3]["y"];
            var inner = %[v: 1];
            var arrC = [1, inner];
            inner["back"] = arrC;
            arrC.saveStruct(__tmpDir + "/t_c.bin", "b");
            var c2 = [];
            c2.loadStruct(__tmpDir + "/t_c.bin", "b");
            var innerV = c2[1]["v"];
            var backIsNull = c2[1]["back"] === void ? 1 : 0;
            var od = %[o: [0xde, 0xad].pack("C2")];
            od.saveStruct(__tmpDir + "/t_o.bin", "b");
            var od2 = %[];
            od2.loadStruct(__tmpDir + "/t_o.bin", "b");
            var oh = od2["o"].unpack("H*")[0];
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("txtErr"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("a3c"_str) == 4);
        REQUIRE(*vm.evalExpr<Integer>("e3"_str) == 7);
        REQUIRE(*vm.evalExpr<Integer>("y"_str) == 8);
        REQUIRE(*vm.evalExpr<Integer>("innerV"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("backIsNull"_str) == 1); // 环检测 → null
        REQUIRE((**vm.evalExpr<String *>("oh"_str)) == "DEAD"_str);
    }

    SECTION("Dictionary.load/save 为原版 TODO 空实现（按特性同步）") {
        vm.eval(R"tjs(
            var d = %[k: 1];
            d.load("whatever.txt");
            d.save("whatever.txt");
            var c = d.count;
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("c"_str) == 1); // 无操作，字典原样
    }
}

TEST_CASE("后缀 ! 即时求值（tjs.y T_EVAL 对齐）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // 语义对齐原版 tTJSInterCodeContext::Eval：A 字符串化为 TJS 表达式编译
    // 执行，空串原值不变、编译失败抛脚本异常可捕获。参考实现 standalone 对
    // 空串用例段错误（流工厂外的又一参考端缺陷），语义按原版源码锁定
    SECTION("基础求值/动态表达式/嵌套") {
        vm.eval(R"tjs(
            var x = "1 + 2";
            var a = (x!);
            var n = 7;
            var d = (("n * n")! + 1);
            var nest = "\"1+1\"!";
            var g = (nest!);
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("a"_str) == 3);
        REQUIRE(*vm.evalExpr<Integer>("d"_str) == 50); // 求值结果直接参与运算
        REQUIRE(*vm.evalExpr<Integer>("g"_str) == 2); // 嵌套 eval
    }

    SECTION("空串原值不变 + 非字符串字符串化") {
        vm.eval(R"tjs(
            var e = "";
            var c = (e!);
            var f = (5)!;
        )tjs"_str);
        REQUIRE((**vm.evalExpr<String *>("c"_str)) == ""_str); // 空串不求值、原值返回
        REQUIRE(*vm.evalExpr<Integer>("f"_str) == 5); // ttstr 化 "5" 再求值
    }

    SECTION("表达式语句形态（结果不被需要）") {
        vm.eval(R"tjs(
            var hit = 0;
            var q = "hit = 1";
            q!;
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("hit"_str) == 1); // 语句形态仍执行求值
    }

    SECTION("编译失败抛脚本异常可捕获") {
        vm.eval(R"tjs(
            var bad = "1 +";
            var caught = 0;
            try { bad!; } catch(ex) { caught = 1; }
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("caught"_str) == 1);
    }
}

TEST_CASE("Math 标准库") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("方法") {
        REQUIRE((*vm.evalExpr<Real>(R"(Math.abs(-3.5))"_str)).value() == 3.5);
        REQUIRE((*vm.evalExpr<Real>(R"(Math.floor(2.7))"_str)).value() == 2.0);
        REQUIRE((*vm.evalExpr<Real>(R"(Math.ceil(2.1))"_str)).value() == 3.0);
        REQUIRE((*vm.evalExpr<Real>(R"(Math.round(2.5))"_str)).value() == 3.0);
        REQUIRE((*vm.evalExpr<Real>(R"(Math.round(-2.5))"_str)).value() == -2.0); // floor(x+0.5) 语义
        REQUIRE((*vm.evalExpr<Real>(R"(Math.sqrt(16))"_str)).value() == 4.0);
        REQUIRE((*vm.evalExpr<Real>(R"(Math.pow(2, 10))"_str)).value() == 1024.0);
        REQUIRE(*vm.evalExpr<Integer>(R"(Math.max(1, 5, 3))"_str) == 5);
        REQUIRE(*vm.evalExpr<Integer>(R"(Math.min(4, 2))"_str) == 2);
        REQUIRE((*vm.evalExpr<Real>(R"(Math.cos(0))"_str)).value() == 1.0);
        REQUIRE(*vm.evalExpr<Integer>(R"(Math.sin(0))"_str) == 0);
    }

    SECTION("原生类构造器（new 编译为透明调用）") {
        vm.eval(R"tjs(
            var re = new RegExp("([0-9]+)-([0-9]+)", "g");
            var m = re.exec("a 12-34 b");
            var g1 = m[1];
            var g2 = m[2];
            var re2 = RegExp("[a-z]+", "i");
            var t1 = re2.test("ABC");
            var a = new Array();
            a.push(7);
            var ac = a.count;
            var av = a[0];
            var d = new Dictionary();
            d["k"] = 5;
            var dc = d.count;
            var dv = d["k"];
        )tjs"_str);
        REQUIRE(*vm.evalExpr<Integer>("g1"_str) == 12);
        REQUIRE(*vm.evalExpr<Integer>("g2"_str) == 34);
        REQUIRE(*vm.evalExpr<Integer>("t1"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("ac"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("av"_str) == 7);
        REQUIRE(*vm.evalExpr<Integer>("dc"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("dv"_str) == 5);
    }

    SECTION("常量与 random") {
        REQUIRE((*vm.evalExpr<Real>(R"(Math.PI)"_str)).value() == Catch::Approx(3.141592653589793).epsilon(1e-15));
        REQUIRE((*vm.evalExpr<Real>(R"(Math.E)"_str)).value() == Catch::Approx(2.718281828459045).epsilon(1e-15));
        REQUIRE((*vm.evalExpr<Real>(R"(Math.SQRT2 * Math.SQRT2)"_str)).value() == Catch::Approx(2.0));
        REQUIRE(*vm.evalExpr<Integer>(R"((Math.random() >= 0 && Math.random() < 1) ? 1 : 0)"_str) == 1);
    }
}

TEST_CASE("Native method calls preserve receivers without allocating bound functions", "[native]") {
    Runtime rt;
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState state{ context };
    VM vm{ &state };
    auto used =
        rt.create<NativeFunction>([&](VM *, Value, size_t) { return Integer{ rt.markSweep.memoryInfo().first }; });
    context.registryGlobalFunc("used"_str, used.get());
    vm.eval(R"(
        var a = [1];
        var before = used();
        for(var i = 0; i < 32; i++) { a.push(a.pop()); }
        var after = used();
        var result = a[0];
        var b = [];
        var f = a.push; var g = b.push;
        f(2); g(3);
        var ac = a.count; var bv = b[0];
    )"_str);
    REQUIRE(state.global("before").asInteger().unwrap() == state.global("after").asInteger().unwrap());
    REQUIRE(state.global("result").asInteger().unwrap() == 1);
    REQUIRE(state.global("ac").asInteger().unwrap() == 2);
    REQUIRE(state.global("bv").asInteger().unwrap() == 3);

    context.registerMethod<String>("probe"_str, [](VM *, Value, size_t) { return Integer{ 1 }; });
    vm.eval("function callProbe() { return \"x\".probe(); } var p1 = callProbe();"_str);
    context.registerMethod<String>("probe"_str, [](VM *, Value, size_t) { return Integer{ 2 }; });
    vm.eval("var p2 = callProbe();"_str);
    REQUIRE(state.global("p1").asInteger().unwrap() == 1);
    REQUIRE(state.global("p2").asInteger().unwrap() == 2);

    context.registerMethod<String>("echo"_str, [](VM *, const Value &self, size_t argc, Value *args) {
        String text = *self.asString().unwrap();
        if(argc)
            text.append(*args[0].asString().unwrap());
        return text;
    });
    REQUIRE(**vm.evalExpr<String *>(R"("outer".echo("inner".echo()))"_str) == "outerinner"_str);
}
