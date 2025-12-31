/*
 * Copyright (c) 2024/12/18
 *
 * 语法解析器全面单元测试
 */

#include <catch.hpp>
#include <memory>

#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"
#include "parser/ast/AstBuilder.hpp"
#include "parser/ast/DeclNode.hpp"
#include "parser/ast/ExprNode.hpp"
#include "parser/ast/StmtNode.hpp"

using namespace Cial;

TEST_CASE("语法解析器 - 表达式解析") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("算术表达式") {
        sourceFile.load(r, "1 + 2 * 3;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        // 验证AST结构
        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *binaryExpr = dynamic_cast<const Syntax::BinaryExprNode *>(exprStmt->expression);
        REQUIRE(binaryExpr != nullptr);
        REQUIRE(*binaryExpr->token->name() == '+');
    }

    SECTION("括号表达式") {
        sourceFile.load(r, "(1 + 2) * 3;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("比较表达式") {
        sourceFile.load(r, "a == b && c > d;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }
}

TEST_CASE("语法解析器 - 变量声明") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("简单变量声明") {
        sourceFile.load(r, "var x = 42;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *varDecl = dynamic_cast<Syntax::VarDeclNode *>(blockNode->childrens[0]);
        REQUIRE(varDecl != nullptr);
        REQUIRE(varDecl->rhs != nullptr);
    }

    SECTION("多个变量声明") {
        sourceFile.load(r, "var a = 1; var b = 2; var c = a + b;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 3);
        for(auto *decl : blockNode->childrens) {
            REQUIRE(dynamic_cast<Syntax::VarDeclNode *>(decl) != nullptr);
        }
    }
}

TEST_CASE("语法解析器 - 控制流语句") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("if语句") {
        sourceFile.load(r, "if (x > 0) { return x; } else { return -x; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *ifStmt = dynamic_cast<const Syntax::IfStmtNode *>(stmtDecl->statement);
        REQUIRE(ifStmt != nullptr);
        REQUIRE(ifStmt->test != nullptr);
        REQUIRE(ifStmt->body != nullptr);
        REQUIRE(ifStmt->elseBody != nullptr);
    }

    SECTION("while循环") {
        sourceFile.load(r, "while (i < 10) { i = i + 1; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        const auto *whileStmt = dynamic_cast<const Syntax::WhileStmtNode *>(stmtDecl->statement);
        REQUIRE(whileStmt != nullptr);
        REQUIRE(whileStmt->test != nullptr);
        REQUIRE(whileStmt->body != nullptr);
    }

    SECTION("break和continue") {
        sourceFile.load(r, "while (true) { if (done) break; continue; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }
}

TEST_CASE("语法解析器 - 函数声明") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("简单函数") {
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
        REQUIRE(funcDecl->body != nullptr);
        REQUIRE(funcDecl->parameters.size() == 2);
    }

    SECTION("函数调用") {
        sourceFile.load(r, "result = add(1, 2);");
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
        auto *assignExpr = dynamic_cast<const Syntax::AssignExprNode *>(exprStmt->expression);
        REQUIRE(assignExpr != nullptr);
        auto *callExpr = dynamic_cast<const Syntax::ProcCallExprNode *>(assignExpr->rhs);
        REQUIRE(callExpr != nullptr);
    }
}

TEST_CASE("语法解析器 - 代码块") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("嵌套代码块") {
        sourceFile.load(r, "{ var x = 1; { var y = 2; x = x + y; } }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        const auto *blockStmt = dynamic_cast<const Syntax::BlockStmtNode *>(stmtDecl->statement);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 2);
    }
}

TEST_CASE("语法解析器 - 错误恢复") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("语法错误") {
        sourceFile.load(r, "var x = ;"); // 缺少表达式
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        // 即使有错误，解析器应该尝试恢复并继续
        REQUIRE(r.isFailed());
    }

    SECTION("不匹配的括号") {
        sourceFile.load(r, "if (x > 0 { return x; }"); // 缺少右括号
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(r.isFailed());
    }
}

TEST_CASE("语法解析器 - 运算符优先级") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("算术运算符优先级") {
        sourceFile.load(r, "1 + 2 * 3;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        // 验证乘法优先级高于加法
        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *binaryExpr = dynamic_cast<const Syntax::BinaryExprNode *>(exprStmt->expression);
        REQUIRE(binaryExpr != nullptr);
        REQUIRE(binaryExpr->token != nullptr);

        // 左操作数应该是字面量1
        const auto *leftValue = dynamic_cast<const Syntax::ValueExprNode *>(binaryExpr->lhs);
        REQUIRE(leftValue != nullptr);

        // 右操作数应该是乘法表达式
        const auto *rightBinary = dynamic_cast<const Syntax::BinaryExprNode *>(binaryExpr->rhs);
        REQUIRE(rightBinary != nullptr);
        REQUIRE(rightBinary->token != nullptr);
    }
}