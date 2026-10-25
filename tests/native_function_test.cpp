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

TEST_CASE("const 语义（对齐原版实测：必须初始化，赋值不强制）") {
    Runtime rt{};
    Context context{ rt };
    context.initNativeMethod();
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    SECTION("const 无初始化为编译期错误") {
        const auto r1 = vm.eval<Value>("const E; return 1;"_str);
        REQUIRE(r1.get().isVoid()); // 编译失败不产出值
        const auto r2 = vm.eval<Value>("const a = 1, b; return 2;"_str);
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
        REQUIRE(*vm.evalExpr<Integer>("pmo"_str) == 6);
        REQUIRE(*vm.evalExpr<Integer>("pd"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("ph"_str) == 12);
        REQUIRE(*vm.evalExpr<Integer>("qy"_str) == 2023); // 1700000000000ms = 2023-11-14
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



