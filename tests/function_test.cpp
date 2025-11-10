/*
 * Copyright (c) 2024/12/18
 *
 * 函数声明单元测试
 */

#include <catch.hpp>

#include "ast/AstBuilder.hpp"
#include "ast/DeclNode.hpp"
#include "ast/ExprNode.hpp"
#include "ast/StmtNode.hpp"
#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"

using namespace Ciallang;

TEST_CASE("函数声明 - 简单函数定义") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("无参数函数") {
        sourceFile.load(r, "function foo() { return 42; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
        REQUIRE(funcDecl != nullptr);
        REQUIRE(funcDecl->token != nullptr);
        REQUIRE(*funcDecl->token->value()->toString() == "foo");
        REQUIRE(funcDecl->parameters.empty());
        REQUIRE(funcDecl->body != nullptr);
        REQUIRE(dynamic_cast<Syntax::BlockStmtNode *>(funcDecl->body) != nullptr);
    }

    SECTION("带参数函数") {
        sourceFile.load(r, "function add(a, b) { return a + b; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
        REQUIRE(funcDecl != nullptr);
        REQUIRE(funcDecl->token != nullptr);
        REQUIRE(*funcDecl->token->value()->toString() == "add");
        REQUIRE(funcDecl->parameters.size() == 2);
        REQUIRE(*funcDecl->parameters[0].first.value()->toString() == "a");
        REQUIRE(*funcDecl->parameters[1].first.value()->toString() == "b");
        REQUIRE(funcDecl->body != nullptr);
    }

    SECTION("空函数体") {
        sourceFile.load(r, "function empty() { }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
        REQUIRE(funcDecl != nullptr);
        auto *body = dynamic_cast<Syntax::BlockStmtNode *>(funcDecl->body);
        REQUIRE(body != nullptr);
        REQUIRE(body->childrens.empty());
    }
}

TEST_CASE("函数声明 - 参数列表") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("单个参数") {
        sourceFile.load(r, "function single(x) { return x; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
        REQUIRE(funcDecl != nullptr);
        REQUIRE(funcDecl->parameters.size() == 1);
        REQUIRE(*funcDecl->parameters[0].first.value()->toString() == "x");
    }

    SECTION("多个参数") {
        sourceFile.load(r, "function multi(a, b, c) { return a + b + c; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
        REQUIRE(funcDecl != nullptr);
        REQUIRE(funcDecl->parameters.size() == 3);
        REQUIRE(*funcDecl->parameters[0].first.value()->toString() == "a");
        REQUIRE(*funcDecl->parameters[1].first.value()->toString() == "b");
        REQUIRE(*funcDecl->parameters[2].first.value()->toString() == "c");
    }

    SECTION("复杂参数名") {
        sourceFile.load(r, "function complex(param1, param_2, param3) { return param1; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
        REQUIRE(funcDecl != nullptr);
        REQUIRE(funcDecl->parameters.size() == 3);
        REQUIRE(*funcDecl->parameters[0].first.value()->toString() == "param1");
        REQUIRE(*funcDecl->parameters[1].first.value()->toString() == "param_2");
        REQUIRE(*funcDecl->parameters[2].first.value()->toString() == "param3");
    }
}

TEST_CASE("函数声明 - 函数体内容") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("带变量声明的函数体") {
        sourceFile.load(r, "function test() { var x = 10; var y = 20; return x + y; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
        REQUIRE(funcDecl != nullptr);
        auto *body = dynamic_cast<Syntax::BlockStmtNode *>(funcDecl->body);
        REQUIRE(body != nullptr);
        REQUIRE(body->childrens.size() == 3);
    }

    SECTION("带控制流的函数体") {
        sourceFile.load(r, "function control() { if (true) return 1; else return 0; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
        REQUIRE(funcDecl != nullptr);
        auto *body = dynamic_cast<Syntax::BlockStmtNode *>(funcDecl->body);
        REQUIRE(body != nullptr);
        REQUIRE(body->childrens.size() == 1);
        REQUIRE(dynamic_cast<const Syntax::IfStmtNode *>(
                    dynamic_cast<const Syntax::StmtDeclNode *>(body->childrens[0])->statement) != nullptr);
    }

    SECTION("带循环的函数体") {
        sourceFile.load(r, "function loop() { while (i < 10) i = i + 1; return i; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
        REQUIRE(funcDecl != nullptr);
        auto *body = dynamic_cast<Syntax::BlockStmtNode *>(funcDecl->body);
        REQUIRE(body != nullptr);
        REQUIRE(body->childrens.size() == 2);
        REQUIRE(dynamic_cast<const Syntax::WhileStmtNode *>(
                    dynamic_cast<const Syntax::StmtDeclNode *>(body->childrens[0])->statement) != nullptr);
    }
}

TEST_CASE("函数声明 - 函数调用") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("无参数函数调用") {
        sourceFile.load(r, "foo();");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *callExpr = dynamic_cast<const Syntax::ProcCallExprNode *>(exprStmt->expression);
        REQUIRE(callExpr != nullptr);
        REQUIRE(callExpr->arguments.empty());
    }

    SECTION("带参数函数调用") {
        sourceFile.load(r, "add(1, 2);");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *callExpr = dynamic_cast<const Syntax::ProcCallExprNode *>(exprStmt->expression);
        REQUIRE(callExpr != nullptr);
        REQUIRE(callExpr->arguments.size() == 2);
    }

    SECTION("嵌套函数调用") {
        sourceFile.load(r, "outer(inner(42));");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *outerCall = dynamic_cast<const Syntax::ProcCallExprNode *>(exprStmt->expression);
        REQUIRE(outerCall != nullptr);
        REQUIRE(outerCall->arguments.size() == 1);
        REQUIRE(dynamic_cast<const Syntax::ProcCallExprNode *>(outerCall->arguments[0]) != nullptr);
    }
}

TEST_CASE("函数声明 - 多个函数定义") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("多个独立函数") {
        sourceFile.load(r, "function a() { return 1; } function b() { return 2; } function c() { return 3; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 3);
        for(auto *decl : blockNode->childrens) {
            auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(decl);
            REQUIRE(funcDecl != nullptr);
        }
    }

    SECTION("函数与变量混合") {
        sourceFile.load(r, "var x = 10; function getX() { return x; } function setX(val) { x = val; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 3);
        REQUIRE(dynamic_cast<Syntax::VarDeclNode *>(blockNode->childrens[0]) != nullptr);
        REQUIRE(dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[1]) != nullptr);
        REQUIRE(dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[2]) != nullptr);
    }
}

TEST_CASE("函数声明 - 递归函数") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("直接递归") {
        sourceFile.load(r, "function factorial(n) { if (n <= 1) return 1; else return n * factorial(n - 1); }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *funcDecl = dynamic_cast<Syntax::FunctionDeclNode *>(blockNode->childrens[0]);
        REQUIRE(funcDecl != nullptr);

        // 检查函数体中是否包含对自身的调用
        auto *body = dynamic_cast<Syntax::BlockStmtNode *>(funcDecl->body);
        REQUIRE(body != nullptr);
        REQUIRE(body->childrens.size() == 1);
        auto *ifStmt = dynamic_cast<const Syntax::IfStmtNode *>(
            dynamic_cast<const Syntax::StmtDeclNode *>((body->childrens[0]))->statement);
        REQUIRE(ifStmt != nullptr);

        // 检查else分支中的递归调用
        REQUIRE(ifStmt->elseBody != nullptr);
        auto *elseBlock = ifStmt->elseBody;
        REQUIRE(elseBlock->childrens.size() == 1);
        auto *elseStmtDecl = dynamic_cast<const Syntax::StmtDeclNode *>(elseBlock->childrens[0]);
        REQUIRE(elseStmtDecl != nullptr);
        auto *elseReturnStmt = dynamic_cast<const Syntax::ReturnStmtNode *>(elseStmtDecl->statement);
        REQUIRE(elseReturnStmt != nullptr);
        REQUIRE(elseReturnStmt->expr != nullptr);
        auto *binaryExpr = dynamic_cast<const Syntax::BinaryExprNode *>(elseReturnStmt->expr);
        REQUIRE(binaryExpr != nullptr);
        REQUIRE(dynamic_cast<const Syntax::ProcCallExprNode *>(binaryExpr->rhs) != nullptr);
    }
}