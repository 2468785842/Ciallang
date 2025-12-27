/*
 * Copyright (c) 2024/12/18
 *
 * 函数声明单元测试
 */

#include <catch.hpp>

#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"
#include "vm/VMState.hpp"

using namespace Cial;

TEST_CASE("函数声明 - 简单函数定义") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    sourceFile.load(r, R"(
        function empty() { }
        function foo() { return 42; }
        function add(a, b) { return a + b; }
        var rEmpty = empty();
        var rFoo = foo();
        var rAdd = add(1, 2);
    )");
    Runtime rt{};
    Syntax::Parser parser{ rt, sourceFile };

    auto *globalNode = parser.parse(r);
    REQUIRE(globalNode != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Inter::IRGenerator codeGen{ sourceFile };
    auto chunk = codeGen.parseAst(r, globalNode);
    REQUIRE(chunk != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Bytecode::VMState vm{ rt };
    vm.allocCallFrame(chunk.get());
    vm.run();

    REQUIRE(vm.global("empty").isObject());
    REQUIRE(vm.global("foo").isObject());
    REQUIRE(vm.global("add").isObject());

    REQUIRE(vm.global("rEmpty").isVoid());

    Value rFoo = vm.global("rFoo");
    REQUIRE(rFoo.isInteger());
    REQUIRE(rFoo.toInteger() == 42);

    Value rAdd = vm.global("rAdd");

    REQUIRE(rAdd.isInteger());
    REQUIRE(rAdd.toInteger() == 3);
}

TEST_CASE("函数声明 - 参数列表") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    sourceFile.load(r, R"(
        function single(x) { return x; } var rSingle = single(1);
        function multi(a, b, c) { return a + b + c; } var rMulti = multi(1, 2, 3);
        function complex(param1, param_2, param3) { return param1; } var rComplex1 = complex(); var rComplex2 = complex(1); var rComplex3 = complex(1, 2, 3);
    )");
    Runtime rt{};
    Syntax::Parser parser{ rt, sourceFile };

    auto *globalNode = parser.parse(r);
    REQUIRE(globalNode != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Inter::IRGenerator codeGen{ sourceFile };
    auto chunk = codeGen.parseAst(r, globalNode);
    REQUIRE(chunk != nullptr);
    REQUIRE_FALSE(r.isFailed());

    Bytecode::VMState vm{ rt };
    vm.allocCallFrame(chunk.get());
    vm.run();

    REQUIRE(vm.global("single").isObject());
    REQUIRE(vm.global("multi").isObject());
    REQUIRE(vm.global("complex").isObject());

    Value rSingle = vm.global("rSingle");
    REQUIRE(rSingle.isInteger());
    REQUIRE(rSingle.toInteger() == 1);

    Value rMulti = vm.global("rMulti");

    REQUIRE(rMulti.isInteger());
    REQUIRE(rMulti.toInteger() == 6);

    Value rComplex1 = vm.global("rComplex1");

    REQUIRE(rComplex1.isVoid());

    Value rComplex2 = vm.global("rComplex2");

    REQUIRE(rComplex2.isInteger());
    REQUIRE(rComplex2.toInteger() == 1);

    Value rComplex3 = vm.global("rComplex3");

    REQUIRE(rComplex3.isInteger());
    REQUIRE(rComplex3.toInteger() == 1);
}

TEST_CASE("函数声明 - 函数体内容") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("带变量声明的函数体") {
        sourceFile.load(r, "function test() { var x = 10; var y = 20; return x + y; } var rTest = test();");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        auto chunk = codeGen.parseAst(r, globalNode);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("test").isObject());

        Value rTest = vm.global("rTest");
        REQUIRE(rTest.isInteger());
        REQUIRE(rTest.toInteger() == 30);
    }

    SECTION("带控制流的函数体") {
        sourceFile.load(r, "function control() { if (true) return 1; else return 0; } var rControl = control();");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        auto chunk = codeGen.parseAst(r, globalNode);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("control").isObject());
        Value rControl = vm.global("rControl");
        REQUIRE(rControl.isInteger());
        REQUIRE(rControl.toInteger() == 1);
    }

    SECTION("带循环的函数体") {
        sourceFile.load(r, "function loop() { var i = 0; while (i < 10) i = i + 1; return i; } var rLoop = loop();");
        Runtime rt{};
        Syntax::Parser parser{ rt, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Inter::IRGenerator codeGen{ sourceFile };
        auto chunk = codeGen.parseAst(r, globalNode);
        REQUIRE(chunk != nullptr);
        REQUIRE_FALSE(r.isFailed());

        Bytecode::VMState vm{ rt };
        vm.allocCallFrame(chunk.get());
        vm.run();
        REQUIRE(vm.global("loop").isObject());
        Value rLoop = vm.global("rLoop");
        REQUIRE(rLoop.isInteger());
        REQUIRE(rLoop.toInteger() == 10);
    }
}

// TEST_CASE("函数声明 - 函数调用") {
//     Common::SourceFile sourceFile{};
//     Common::Result r{};
//     Syntax::AstBuilder astBuilder{};
//
//     SECTION("无参数函数调用") {
//         sourceFile.load(r, "foo();");
//         Syntax::Parser parser{ sourceFile, astBuilder };
//
//         auto *globalNode = parser.parse(r);
//         REQUIRE(globalNode != nullptr);
//         REQUIRE_FALSE(r.isFailed());
//
//         auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
//         REQUIRE(blockNode != nullptr);
//         REQUIRE(blockNode->childrens.size() == 1);
//         auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
//         REQUIRE(stmtDecl != nullptr);
//         auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
//         REQUIRE(exprStmt != nullptr);
//         auto *callExpr = dynamic_cast<const Syntax::ProcCallExprNode *>(exprStmt->expression);
//         REQUIRE(callExpr != nullptr);
//         REQUIRE(callExpr->arguments.empty());
//     }
//
//     SECTION("带参数函数调用") {
//         sourceFile.load(r, "add(1, 2);");
//         Syntax::Parser parser{ sourceFile, astBuilder };
//
//         auto *globalNode = parser.parse(r);
//         REQUIRE(globalNode != nullptr);
//         REQUIRE_FALSE(r.isFailed());
//
//         auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
//         REQUIRE(blockNode != nullptr);
//         REQUIRE(blockNode->childrens.size() == 1);
//         auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
//         REQUIRE(stmtDecl != nullptr);
//         auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
//         REQUIRE(exprStmt != nullptr);
//         auto *callExpr = dynamic_cast<const Syntax::ProcCallExprNode *>(exprStmt->expression);
//         REQUIRE(callExpr != nullptr);
//         REQUIRE(callExpr->arguments.size() == 2);
//     }
//
//     SECTION("嵌套函数调用") {
//         sourceFile.load(r, "outer(inner(42));");
//         Syntax::Parser parser{ sourceFile, astBuilder };
//
//         auto *globalNode = parser.parse(r);
//         REQUIRE(globalNode != nullptr);
//         REQUIRE_FALSE(r.isFailed());
//
//         auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
//         REQUIRE(blockNode != nullptr);
//         REQUIRE(blockNode->childrens.size() == 1);
//         auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
//         REQUIRE(stmtDecl != nullptr);
//         auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
//         REQUIRE(exprStmt != nullptr);
//         auto *outerCall = dynamic_cast<const Syntax::ProcCallExprNode *>(exprStmt->expression);
//         REQUIRE(outerCall != nullptr);
//         REQUIRE(outerCall->arguments.size() == 1);
//         REQUIRE(dynamic_cast<const Syntax::ProcCallExprNode *>(outerCall->arguments[0]) != nullptr);
//     }
// }
//
// TEST_CASE("函数声明 - 多个函数定义") {
//     Common::SourceFile sourceFile{};
//     Common::Result r{};
//     Syntax::AstBuilder astBuilder{};
//
//     SECTION("多个独立函数") {
//         sourceFile.load(r, "function a() { return 1; } function b() { return 2; } function c() { return 3; }");
//         Syntax::Parser parser{ sourceFile, astBuilder };
//
//         auto *globalNode = parser.parse(r);
//         REQUIRE(globalNode != nullptr);
//         REQUIRE_FALSE(r.isFailed());
//
//         auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
//         REQUIRE(blockNode != nullptr);
//         REQUIRE(blockNode->childrens.size() == 3);
//         for(auto *decl : blockNode->childrens) {
//             auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(decl);
//             REQUIRE(funcDecl != nullptr);
//         }
//     }
//
//     SECTION("函数与变量混合") {
//         sourceFile.load(r, "var x = 10; function getX() { return x; } function setX(val) { x = val; }");
//         Syntax::Parser parser{ sourceFile, astBuilder };
//
//         auto *globalNode = parser.parse(r);
//         REQUIRE(globalNode != nullptr);
//         REQUIRE_FALSE(r.isFailed());
//
//         auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
//         REQUIRE(blockNode != nullptr);
//         REQUIRE(blockNode->childrens.size() == 3);
//         REQUIRE(dynamic_cast<Syntax::VarDeclNode *>(blockNode->childrens[0]) != nullptr);
//         REQUIRE(dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[1]) != nullptr);
//         REQUIRE(dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[2]) != nullptr);
//     }
// }
//
// TEST_CASE("函数声明 - 递归函数") {
//     Common::SourceFile sourceFile{};
//     Common::Result r{};
//     Syntax::AstBuilder astBuilder{};
//
//     SECTION("直接递归") {
//         sourceFile.load(r, "function factorial(n) { if (n <= 1) return 1; else return n * factorial(n - 1); }");
//         Syntax::Parser parser{ sourceFile, astBuilder };
//
//         auto *globalNode = parser.parse(r);
//         REQUIRE(globalNode != nullptr);
//         REQUIRE_FALSE(r.isFailed());
//
//         auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
//         REQUIRE(blockNode != nullptr);
//         REQUIRE(blockNode->childrens.size() == 1);
//         auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
//         REQUIRE(funcDecl != nullptr);
//
//         // 检查函数体中是否包含对自身的调用
//         auto *body = dynamic_cast<Syntax::BlockStmtNode *>(funcDecl->body);
//         REQUIRE(body != nullptr);
//         REQUIRE(body->childrens.size() == 1);
//         auto *ifStmt = dynamic_cast<const Syntax::IfStmtNode *>(
//             dynamic_cast<const Syntax::StmtDeclNode *>((body->childrens[0]))->statement);
//         REQUIRE(ifStmt != nullptr);
//
//         // 检查else分支中的递归调用
//         REQUIRE(ifStmt->elseBody != nullptr);
//         auto *elseBlock = ifStmt->elseBody;
//         REQUIRE(elseBlock->childrens.size() == 1);
//         auto *elseStmtDecl = dynamic_cast<const Syntax::StmtDeclNode *>(elseBlock->childrens[0]);
//         REQUIRE(elseStmtDecl != nullptr);
//         auto *elseReturnStmt = dynamic_cast<const Syntax::ReturnStmtNode *>(elseStmtDecl->statement);
//         REQUIRE(elseReturnStmt != nullptr);
//         REQUIRE(elseReturnStmt->expr != nullptr);
//         auto *binaryExpr = dynamic_cast<const Syntax::BinaryExprNode *>(elseReturnStmt->expr);
//         REQUIRE(binaryExpr != nullptr);
//         REQUIRE(dynamic_cast<const Syntax::ProcCallExprNode *>(binaryExpr->rhs) != nullptr);
//     }
// }