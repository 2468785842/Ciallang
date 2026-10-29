/*
 * Copyright (c) 2026/10/29
 *
 * 保留字与文档性语法的行为锁定测试（对齐原版 TJS2 实测行为）
 */

#include <catch.hpp>

#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"
#include "runtime/Context.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

// ── `in` 运算符（tjs.y:661，保留字）──
// 原版 tjsInterCodeGen 无 T_IN 分支：节点与两侧操作数均不求值，整体为 void。
// 参考实现实测（2026-10-29）：`var x = "a" in %[a:1];` → x 为 void；
// `if("a" in d)` 恒假；官方リファレンス亦无 in 运算符章节。
TEST_CASE("保留字 - in 运算符为无代码 void") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        var count = 0;
        function f() { count++; return "a"; }
        var d = %[a:1];
        var x = f() in d;
        var inCond = 0;
        if("a" in d) inCond = 1;
    )"_str);

    SECTION("表达式结果为 void") { REQUIRE(*vm.evalExpr<Integer>("x === void"_str) == 1); }

    SECTION("两侧操作数不求值") { REQUIRE(*vm.evalExpr<Integer>("count"_str) == 0); }

    SECTION("作条件恒假") { REQUIRE(*vm.evalExpr<Integer>("inCond"_str) == 0); }

    SECTION("嵌套时 in 子式等价 void 字面量") {
        // 原版 `1 + (d in d)` 生成 `add %r, %0(void)`：与 1 + void 完全同码
        REQUIRE(*vm.evalExpr<Integer>("(1 + (d in d)) === (1 + void)"_str) == 1);
    }
}

// ── 类型注解（tjs.y:339 variable_type）──
// 原版注释 "It is not currently effect. Ignore types."：`: symbol|void|int|real|string|octet`
// 出现在 var 声明/函数参数/函数返回值/getter/setter，全部仅解析并丢弃。
TEST_CASE("类型注解 - 解析接受且无语义") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("var 声明注解") {
        sourceFile.load(r, "var a : int = 3; var b : void; var c : MyClass;");
        Runtime rt{};
        Context context{ rt };
        syntax::Parser parser{ sourceFile, context.pp() };
        REQUIRE(parser.parse(r) != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("for 初始化注解") {
        sourceFile.load(r, "for(var i : int = 0; i < 3; i++) ;");
        Runtime rt{};
        Context context{ rt };
        syntax::Parser parser{ sourceFile, context.pp() };
        // 注解本身可解析；`;` 空语句体为 Ciallang 既有不支持（与注解无关），
        // 期望错误仅来自空语句，不来自 `: int`
        parser.parse(r);
        REQUIRE(r.isFailed());
    }

    SECTION("for 初始化注解带语句体") {
        // for 体为 var 声明是 Ciallang 既有不支持（if/for 单语句体同），与注解无关
        sourceFile.load(r, "for(var i : int = 0; i < 3; i++) t = i;");
        Runtime rt{};
        Context context{ rt };
        syntax::Parser parser{ sourceFile, context.pp() };
        REQUIRE(parser.parse(r) != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("函数参数/返回值注解") {
        sourceFile.load(r, "function f(x : real, y : string = \"s\") : void { return x; }");
        Runtime rt{};
        Context context{ rt };
        syntax::Parser parser{ sourceFile, context.pp() };
        REQUIRE(parser.parse(r) != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("函数表达式注解") {
        sourceFile.load(r, "var g = function(m : int) : int { return m + 1; };");
        Runtime rt{};
        Context context{ rt };
        syntax::Parser parser{ sourceFile, context.pp() };
        REQUIRE(parser.parse(r) != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("getter/setter 注解（tjs.y:430/450）") {
        sourceFile.load(r, "property pp { getter() : int { return 1; } setter(v : int) { } }");
        Runtime rt{};
        Context context{ rt };
        syntax::Parser parser{ sourceFile, context.pp() };
        REQUIRE(parser.parse(r) != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("注解后行为不受影响") {
        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        vm.eval(R"(
            var y : int = 7;
            function g(p : real, q : string = "s") : void { return p; }
            var h = function(m : int) : int { return m + 1; };
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("y"_str) == 7);
        REQUIRE((*vm.evalExpr<Real>("g(1.5)"_str)).value() == Catch::Approx(1.5));
        REQUIRE(*vm.evalExpr<Integer>("h(4)"_str) == 5);
    }
}

// ── new 操作数必须是调用（tjs.y:659 `"new" func_call_expr`）──
// 裸 `new A`（无括号）原版为语法错误（实测 "syntax error, unexpected ;"）；
// `new A().m` 等价 (new A()).m（后缀 Dot/Index 豁免）。
TEST_CASE("new - 无调用形态为编译错误") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("裸 new 类名") {
        sourceFile.load(r, "class A { var x = 1; } var a = new A;");
        Runtime rt{};
        Context context{ rt };
        syntax::Parser parser{ sourceFile, context.pp() };
        parser.parse(r);
        REQUIRE(r.isFailed());
    }

    SECTION("new 成员访问无调用") {
        sourceFile.load(r, "class A {} var a = new A.m;");
        Runtime rt{};
        Context context{ rt };
        syntax::Parser parser{ sourceFile, context.pp() };
        parser.parse(r);
        REQUIRE(r.isFailed());
    }

    SECTION("new 调用与后缀豁免仍可用") {
        Runtime rt{};
        Context context{ rt };
        vm::VMState vmState{ context };
        VM vm{ &vmState };
        vm.eval(R"(
            class A { var x = 1; function m() { return 5; } }
        )"_str);
        REQUIRE(*vm.evalExpr<Integer>("new A().x"_str) == 1);
        REQUIRE(*vm.evalExpr<Integer>("new A().m()"_str) == 5);
    }
}

// ── 模板串内嵌表达式（官方 Token.md：@ 字符串字面量）──
// `&expr;` 与 `${expr}` 求值展开；`\&`/`\$` 转义不展开；裸 `$`（无紧跟 `{`）
// 为普通文本（参考 tjsLex.cpp:315 '$' 分支）。
TEST_CASE("模板串 - 内嵌表达式展开") {
    Runtime rt{};
    Context context{ rt };
    vm::VMState vmState{ context };
    VM vm{ &vmState };

    vm.eval(R"(
        var x = 3;
        function f() { return 7; }
    )"_str);

    SECTION("&表达式; 展开") { REQUIRE(*(*vm.evalExpr<String *>(R"(@"1+2=&1+2;")"_str)) == "1+2=3"); }

    SECTION("&局部变量; 与 &函数调用;") { REQUIRE(*(*vm.evalExpr<String *>(R"(@"x=&x; f=&f();")"_str)) == "x=3 f=7"); }

    SECTION("${表达式} 展开") { REQUIRE(*(*vm.evalExpr<String *>(R"(@"v=${f()}")"_str)) == "v=7"); }

    SECTION("转义 \\& 不展开") { REQUIRE(*(*vm.evalExpr<String *>(R"(@"lit \& end")"_str)) == "lit & end"); }

    SECTION("裸 $ 为普通文本") { REQUIRE(*(*vm.evalExpr<String *>(R"(@"d $x; end")"_str)) == "d $x; end"); }
}
