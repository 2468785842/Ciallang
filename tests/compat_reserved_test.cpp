/*
 * Copyright (c) 2026/10/29
 *
 * 保留字与文档性语法的行为锁定测试（对齐原版 TJS2 实测行为）
 */

#include <catch2/catch_all.hpp>

#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"
#include "runtime/Context.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

using namespace cial;

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

// ── static/private/public/protected（tjs.y:116-120、tjsLex.cpp:926）──
// 原版保留字但文法产生式零使用（与 `in` 同类）：类内 `static var` 等写法
// 在原版即语法错误。Ciallang 同样拒绝编译——行为锁定，勿实现成 static 成员。
TEST_CASE("保留字 - static/private/public/protected 无文法规则") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    for(const char *decl :
        { "class C { static var sv = 1; }", "class C { static function f() { } }", "class C { private var pv = 1; }",
          "class C { public var uv = 1; }", "class C { protected var xv = 1; }", "static var top = 1;" }) {
        sourceFile.load(r, decl);
        Runtime rt{};
        Context context{ rt };
        syntax::Parser parser{ sourceFile, context.pp() };
        parser.parse(r);
        INFO("source: " << decl);
        REQUIRE(r.isFailed());
    }
}

TEST_CASE("constant containers reject runtime expressions", "[parser]") {
    for(const auto *source : { "var a = (const)[x];", "var a = (const)[1 + 2];", "var a = (const)[[]];",
                               "var a = (const)[,];", "var a = (const)[1,];", "var a = (const)%[key: 1];",
                               "var a = (const)%[1 => 2];", "var a = (const)(1);", "var a = (const)[-(1)];", "var a = (const)%[,];" }) {
        INFO(source);
        Common::SourceFile file;
        Common::Result diagnostics;
        file.load(diagnostics, source);
        Runtime rt;
        Context context{ rt };
        syntax::Parser parser{ file, context.pp() };
        parser.parse(diagnostics);
        REQUIRE(diagnostics.isFailed());
    }
}
