/*
 * Copyright (c) 2026/1/4
 *
 * 本地函数单元测试
 */

#include <catch2/catch_all.hpp>
#include "stdlib/StandardLibrary.hpp"

#include "StderrCapture.hpp"

#include <filesystem>

#include <fmt/core.h>

#include "stdlib/RandomLib.hpp"
#include "stdlib/StringLib.hpp"
#include "types/Dictionary.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;
using namespace cial::stdlib;

TEST_CASE("String search and slices reject unsafe bounds", "[stdlib][regression]") {
    Runtime rt;
    Context context{ rt };
    registerStandardLibrary(context);
    vm::VMState state{ context };
    VM vm{ &state };
    vm.eval(R"(var text = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl";)"_str);
    REQUIRE(*vm.evalExpr<Integer>(R"(text.indexOf("b", -1))"_str) == -1);
    REQUIRE(*vm.evalExpr<Integer>(R"(text.indexOf("b", -9223372036854775807 - 1))"_str) == -1);
    REQUIRE(*vm.evalExpr<Integer>(R"(text.indexOf("b", 0))"_str) == 1);
    REQUIRE(*vm.evalExpr<Integer>(R"(text.indexOf("b", 2))"_str) == 27);
    REQUIRE(*vm.evalExpr<Integer>(R"(text.indexOf("b", 9223372036854775807))"_str) == -1);
    REQUIRE(**vm.evalExpr<String *>(R"("abc".substring(1, 9223372036854775807))"_str) == "bc"_str);
    REQUIRE(**vm.evalExpr<String *>(R"("abc".substr(2, 9223372036854775807))"_str) == "c"_str);
    REQUIRE(**vm.evalExpr<String *>(R"("abc".substring(1, -1))"_str) == ""_str);
}

TEST_CASE("Random state restoration validates metadata and preserves rejected state", "[stdlib][regression]") {
    Runtime rt;
    Context context{ rt };
    registerStandardLibrary(context);
    vm::VMState state{ context };
    VM vm{ &state };

    SECTION("valid checkpoints span initialization and block boundaries") {
        for(const int count : { 0, 1, 623, 624, 625 }) {
            CAPTURE(count);
            vm.eval(String{ fmt::format(R"(
                var source = new Math.RandomGenerator(123);
                for(var i = 0; i < {}; i++) source.random32();
                var restored = new Math.RandomGenerator(source.serialize());
                var equal = 1;
                for(var j = 0; j < 630; j++)
                    if(source.random32() != restored.random32()) equal = 0;
            )",
                                        count) });
            REQUIRE(*vm.evalExpr<Integer>("equal"_str) == 1);
        }
    }

    SECTION("invalid metadata is rejected before narrowing or modifying the generator") {
        vm.eval(R"(
            var r = new Math.RandomGenerator(123);
            var expected = new Math.RandomGenerator(r.serialize());
        )"_str);
        const std::pair<Integer, Integer> invalid[] = {
            { 2, 624 }, { 0, 0 },   { -1, 0 },           { 625, 0 },          { 1, -1 },
            { 1, 625 }, { 624, 2 }, { 4294967297LL, 0 }, { 1, 4294967296LL }, { 9223372036854775807LL, 0 },
        };
        for(const auto &[left, next] : invalid) {
            CAPTURE(left, next);
            vm.eval(String{ fmt::format("var bad = r.serialize(); bad.left = {}; bad.next = {};", left, next) });
            REQUIRE_THROWS(vm.eval<Value>("r.randomize(bad);"_str));
            REQUIRE_THROWS(vm.eval<Value>("var rejected = new Math.RandomGenerator(bad);"_str));
            REQUIRE(*vm.evalExpr<Integer>("r.random32() == expected.random32()"_str) == 1);
        }
    }

    SECTION("a malformed late hex digit cannot partially overwrite the current state") {
        vm.eval(R"(
            var r = new Math.RandomGenerator(123);
            var expected = new Math.RandomGenerator(r.serialize());
            var bad = r.serialize();
            bad.state = "00000000" + bad.state.substring(8, 4983) + "z";
        )"_str);
        REQUIRE_THROWS(vm.eval<Value>("r.randomize(bad);"_str));
        vm.eval(R"(
            var equal = 1;
            for(var i = 0; i < 630; i++)
                if(r.random32() != expected.random32()) equal = 0;
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("equal"_str) == 1);
    }

    SECTION("the public restore entry validates indices") {
        RandomGen generator;
        const String hex{ std::string(RandomGen::MT_N * 8, '0') };
        REQUIRE_THROWS_AS(generator.restoreFrom(hex, 2, RandomGen::MT_N), std::runtime_error);
        REQUIRE_THROWS_AS(generator.restoreFrom(hex, 0, 0), std::runtime_error);
        REQUIRE_NOTHROW(generator.restoreFrom(hex, 1, 0));
        REQUIRE(generator.random32() == 0);
    }
}

TEST_CASE("Floating unpack consumes each field once and bounds truncated inputs", "[stdlib][regression]") {
    Runtime rt;
    Context context{ rt };
    registerStandardLibrary(context);
    vm::VMState state{ context };
    VM vm{ &state };
    for(const char *format : { "dC", "fC", "dCdC", "fCfC", "d2C", "f2C" }) {
        CAPTURE(format);
        const bool interleaved = std::strlen(format) == 4;
        const bool repeated = format[1] == '2';
        const auto values = interleaved ? "[1.5,7,2.5,9]" : repeated ? "[1.5,2.5,7]" : "[1.5,7]";
        const auto expected = interleaved ? "1.5,7,2.5,9" : repeated ? "1.5,2.5,7" : "1.5,7";
        const String expr{ fmt::format(R"({}.pack("{}").unpack("{}").join(","))", values, format, format) };
        REQUIRE((**vm.evalExpr<String *>(expr)).toStdStr() == expected);
    }
    // 不完整的浮点字段仍按既有语义补零；后续字段不能读取缓冲区之外的字节。
    for(const char *format : { "d2C", "f2C", "d2147483647C", "f2147483647C" }) {
        CAPTURE(format);
        vm.eval(String{ fmt::format(R"(var partial = [0,0,0].pack("C3").unpack("{}");)", format) });
        REQUIRE(*vm.evalExpr<Integer>("partial.count"_str) == 1);
        REQUIRE((*vm.evalExpr<Real>("partial[0]"_str)).value() == 0.0);
    }
}

TEST_CASE("Native receiver snapshots survive rebinding and nested callbacks", "[native]") {
    Runtime runtime;
    const Value outer{ "outer receiver longer than inline storage"_str };
    const Value inner{ "inner receiver longer than inline storage"_str };

    SECTION("reference callback keeps its snapshot during a nested call") {
        NativeFunction *function{};
        bool nested = false;
        auto rooted = runtime.create<NativeFunction>([&](VM *, const Value &self, size_t) {
            if(!nested) {
                nested = true;
                function->setThisObj(inner);
                const auto value = function->callProc(nullptr, 0, nullptr);
                REQUIRE(*value.asString().unwrap() == *inner.asString().unwrap());
            }
            return self;
        });
        function = rooted.get();
        function->setThisObj(outer);
        const auto value = function->callProc(nullptr, 0, nullptr);
        REQUIRE(*value.asString().unwrap() == *outer.asString().unwrap());
        REQUIRE(*function->boundThis().asString().unwrap() == *inner.asString().unwrap());
    }

    SECTION("value callback can replace its local receiver") {
        auto rooted = runtime.create<NativeFunction>([&](VM *, Value self, size_t) {
            self = inner;
            return self;
        });
        rooted->setThisObj(outer);
        const auto value = rooted->callProc(nullptr, 0, nullptr);
        REQUIRE(*value.asString().unwrap() == *inner.asString().unwrap());
        REQUIRE(*rooted->boundThis().asString().unwrap() == *outer.asString().unwrap());
    }
}

TEST_CASE("字符串拼接 SSO 边界回归（SHORT_STR_LEN 一致性）") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // 修复：concat 的 SSO 阈值(total <= 21)曾与构造/析构/move 的长串判据(len > 21)
    // 不一致——恰好 21 字符的结果丢尾字符并泄漏缓冲
    SECTION("链式拼接跨 20/21/22 字符边界") {
        REQUIRE((**vm.evalExpr<String *>(R"("1234567890123456789" + "0")"_str)) == "12345678901234567890"_str); // 20
        REQUIRE((**vm.evalExpr<String *>(R"("1234567890123456789" + "01")"_str)) == "123456789012345678901"_str); // 21
        REQUIRE((**vm.evalExpr<String *>(R"("1234567890123456789" + "012")"_str)) ==
                "1234567890123456789012"_str); // 22
        REQUIRE((**vm.evalExpr<String *>(R"("hit: " + "x22x" + " idx=" + 2 + " zzz=" + "6")"_str)) ==
                "hit: x22x idx=2 zzz=6"_str);
        REQUIRE(*vm.evalExpr<Integer>(R"(("hit: " + "x22x" + " idx=" + 2 + " zzz=" + "6").length)"_str) == 21);
    }
}

TEST_CASE("原生回调脚本函数（VM::callValue 底座）") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
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
        REQUIRE((**vm.evalExpr<String *>(
                    R"("a1b22c".replace(/\d+/g, function(m) { return "<" + m[0] + ">"; }))"_str)) == "a<1>b<22>c"_str);
        REQUIRE((**vm.evalExpr<String *>(R"("x7y".replace(/(\d)/, function(m) { return m[1] + m[1]; }))"_str)) ==
                "x77y"_str);
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

TEST_CASE("(const)[...] / (const)%[...] 常量内联形态（当前实现：可写的运行期容器）") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    // 参考 const_inline_array 在解析期建容器，再经 VM_CONST 复用常量对象。
    // 当前 Parser 忽略 (const)，此处只覆盖可写性及嵌套语法，不证明跨调用共享已对齐。
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
    stdlib::registerStandardLibrary(context);
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

TEST_CASE("Date.parse 完整文法（tjsdate.y 全形态，对拍 TJSCompiler 一致）") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
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

TEST_CASE("Array/Dictionary 结构化存取 + Octet pack 系（tjsBinarySerializer/tjsOctPack 对齐）") {
    Runtime rt{};
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
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

TEST_CASE("Native method calls preserve receivers without allocating bound functions", "[native]") {
    Runtime rt;
    Context context{ rt };
    stdlib::registerStandardLibrary(context);
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

TEST_CASE("RegExp replacement callbacks preserve bindings and use regexp as fallback", "[native][regexp]") {
    Runtime runtime;
    Context context{ runtime };
    stdlib::registerStandardLibrary(context);
    vm::VMState state{ context };
    VM vm{ &state };

    SECTION("unbound expressions receive the regexp for both replace entry points") {
        vm.eval(R"(
            var re = /x/g;
            var direct = re.replace("xx", function(m) {
                return this instanceof "RegExp" ? "R" : "bad";
            });
            var stringEntry = "xx".replace(re, function(m) {
                return this instanceof "RegExp" ? "S" : "bad";
            });
        )"_str);
        REQUIRE(**vm.evalExpr<String *>("direct"_str) == "RR"_str);
        REQUIRE(**vm.evalExpr<String *>("stringEntry"_str) == "SS"_str);
    }

    SECTION("global function declarations retain their global binding") {
        vm.eval(R"(
            var start = 41;
            function replacement(m) { return this.start; }
            var result = /x/g.replace("xx", replacement);
        )"_str);
        REQUIRE(**vm.evalExpr<String *>("result"_str) == "4141"_str);
    }

    SECTION("an explicitly bound function keeps its receiver") {
        vm.eval(R"(
            var holder = %[marker: "B"];
            var replacement = (function(m) { return this.marker; }) incontextof holder;
            var result = /x/g.replace("xx", replacement);
        )"_str);
        REQUIRE(**vm.evalExpr<String *>("result"_str) == "BB"_str);
    }

    SECTION("nested global replacements do not overwrite the outer receiver") {
        vm.eval(R"(
            var outer = /x/g;
            var inner = /y/g;
            outer.test("Ox");
            inner.test("Iy");
            outer.start = 0;
            inner.start = 0;
            var result = outer.replace("xx", function(m) {
                var before = this.input;
                var middle = inner.replace("yy", function(n) { return this.input; });
                return before + middle + this.input;
            });
        )"_str);
        REQUIRE(**vm.evalExpr<String *>("result"_str) == "OxIyIyOxOxIyIyOx"_str);
        REQUIRE(*vm.evalExpr<Integer>("outer.start"_str) == 0);
        REQUIRE(*vm.evalExpr<Integer>("inner.start"_str) == 0);
    }

    SECTION("native callbacks also distinguish fallback and explicit bindings") {
        vm.eval("var nativeRegexp = /x/g;"_str);
        Object *expected = state.global("nativeRegexp").asObject().unwrap();
        Vec<Object *> receivers;
        Vec<size_t> argumentCounts;
        auto callback = runtime.create<NativeFunction>([&](VM *, const Value &self, size_t argc, Value *) {
            receivers.push_back(self.isObject() ? self.asObject().unwrap() : nullptr);
            argumentCounts.push_back(argc);
            return "N"_str;
        });
        auto holder = runtime.create<Dictionary>();
        SECTION("unbound") {}
        SECTION("bound") {
            expected = holder.get();
            callback->setThisObj(Value{ expected });
        }
        context.registryGlobalFunc("nativeReplacement"_str, callback.get());
        vm.eval("var result = nativeRegexp.replace(\"xx\", nativeReplacement);"_str);
        REQUIRE(**vm.evalExpr<String *>("result"_str) == "NN"_str);
        REQUIRE(receivers == Vec<Object *>{ expected, expected });
        REQUIRE(argumentCounts == Vec<size_t>{ 1, 1 });
    }
}
