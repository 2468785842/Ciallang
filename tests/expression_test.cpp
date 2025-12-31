/*
 * Copyright (c) 2024/12/18
 *
 * 表达式单元测试
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

TEST_CASE("表达式 - 字面量表达式") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    AtomTable atomTable{};

    SECTION("整数字面量") {
        sourceFile.load(r, "123;");
        Syntax::Parser parser{ atomTable, sourceFile };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockStmt = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockStmt->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *valueExpr = dynamic_cast<const Syntax::ValueExprNode *>(exprStmt->expression);
        REQUIRE(valueExpr != nullptr);
        REQUIRE(valueExpr->token->type() == Syntax::TokenType::ConstVal);
        REQUIRE(valueExpr->token->value().toInteger() == 123);
    }

    SECTION("浮点数字面量") {
        sourceFile.load(r, "3.14;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockStmt = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockStmt->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *valueExpr = dynamic_cast<const Syntax::ValueExprNode *>(exprStmt->expression);
        REQUIRE(valueExpr != nullptr);
        REQUIRE(valueExpr->token->type() == Syntax::TokenType::ConstVal);
        REQUIRE(valueExpr->token->value().toReal() == Catch::Approx(3.14));
    }

    SECTION("字符串字面量") {
        sourceFile.load(r, R"("hello world";)");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockStmt = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockStmt->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *valueExpr = dynamic_cast<const Syntax::ValueExprNode *>(exprStmt->expression);
        REQUIRE(valueExpr != nullptr);
        REQUIRE(valueExpr->token->type() == Syntax::TokenType::ConstVal);
        REQUIRE(valueExpr->token->value().toString()->toStdStr() == "hello world");
    }
}

TEST_CASE("表达式 - 标识符表达式") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("变量引用") {
        sourceFile.load(r, "variableName;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockStmt = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockStmt->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *identExpr = dynamic_cast<const Syntax::IdentifierExprNode *>(exprStmt->expression);
        REQUIRE(identExpr != nullptr);
        REQUIRE(identExpr->token != nullptr);
        // Token的name()返回token类型名，不是标识符值，这里只验证token存在
        REQUIRE(identExpr->token->type() == Syntax::TokenType::Identifier);
    }
}

TEST_CASE("表达式 - 二元运算表达式") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("算术运算") {
        sourceFile.load(r, "a + b;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockStmt = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockStmt->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *binaryExpr = dynamic_cast<const Syntax::BinaryExprNode *>(exprStmt->expression);
        REQUIRE(binaryExpr != nullptr);
        REQUIRE(binaryExpr->token->type() == Syntax::TokenType::Plus);
        REQUIRE(dynamic_cast<const Syntax::IdentifierExprNode *>(binaryExpr->lhs) != nullptr);
        REQUIRE(dynamic_cast<const Syntax::IdentifierExprNode *>(binaryExpr->rhs) != nullptr);
    }

    SECTION("比较运算") {
        sourceFile.load(r, "x > y;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockStmt = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockStmt->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *binaryExpr = dynamic_cast<const Syntax::BinaryExprNode *>(exprStmt->expression);
        REQUIRE(binaryExpr != nullptr);
        REQUIRE(binaryExpr->token->type() == Syntax::TokenType::Gt);
    }

    SECTION("逻辑运算") {
        sourceFile.load(r, "a && b;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockStmt = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockStmt->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *binaryExpr = dynamic_cast<const Syntax::BinaryExprNode *>(exprStmt->expression);
        REQUIRE(binaryExpr != nullptr);
        REQUIRE(binaryExpr->token->type() == Syntax::TokenType::LogicalAnd);
    }
}

TEST_CASE("表达式 - 一元运算表达式") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("逻辑非") {
        sourceFile.load(r, "!flag;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockStmt = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockStmt->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *unaryExpr = dynamic_cast<const Syntax::UnaryExprNode *>(exprStmt->expression);
        REQUIRE(unaryExpr != nullptr);
        REQUIRE(unaryExpr->token->type() == Syntax::TokenType::Exclamation);
        REQUIRE(dynamic_cast<const Syntax::IdentifierExprNode *>(unaryExpr->rhs) != nullptr);
    }

    SECTION("负号") {
        sourceFile.load(r, "-value;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockStmt = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockStmt->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *unaryExpr = dynamic_cast<const Syntax::UnaryExprNode *>(exprStmt->expression);
        REQUIRE(unaryExpr != nullptr);
        REQUIRE(unaryExpr->token->type() == Syntax::TokenType::Minus);
    }
}

TEST_CASE("表达式 - 赋值表达式") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("简单赋值") {
        sourceFile.load(r, "x = 42;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockStmt = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockStmt->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *exprStmt = dynamic_cast<const Syntax::ExprStmtNode *>(stmtDecl->statement);
        REQUIRE(exprStmt != nullptr);
        auto *assignExpr = dynamic_cast<const Syntax::AssignExprNode *>(exprStmt->expression);
        REQUIRE(assignExpr != nullptr);
        REQUIRE(dynamic_cast<const Syntax::IdentifierExprNode *>(assignExpr->lhs) != nullptr);
        REQUIRE(dynamic_cast<const Syntax::ValueExprNode *>(assignExpr->rhs) != nullptr);
    }

    SECTION("复合赋值") {
        sourceFile.load(r, "x = y = 10;");
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
        REQUIRE(dynamic_cast<const Syntax::IdentifierExprNode *>(assignExpr->lhs) != nullptr);
        REQUIRE(dynamic_cast<const Syntax::AssignExprNode *>(assignExpr->rhs) != nullptr);
    }
}

TEST_CASE("表达式 - 函数调用表达式") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("无参数调用") {
        sourceFile.load(r, "func();");
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

    SECTION("带参数调用") {
        sourceFile.load(r, "add(1, 2, 3);");
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
        REQUIRE(callExpr->arguments.size() == 3);
    }

    SECTION("嵌套调用") {
        sourceFile.load(r, "max(min(a, b), c);");
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

    SECTION("逗号表示隐式void参数 - 两个逗号") {
        sourceFile.load(r, "func(,);");
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

    SECTION("逗号表示隐式void参数 - 三个逗号") {
        sourceFile.load(r, "func(,,);");
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
        REQUIRE(callExpr->arguments.size() == 3);
    }

    SECTION("逗号表示隐式void参数 - 参数后跟逗号") {
        sourceFile.load(r, "func(2,);");
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

    SECTION("逗号表示隐式void参数 - 逗号后跟参数") {
        sourceFile.load(r, "func(,2);");
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

    SECTION("逗号表示隐式void参数 - 混合情况") {
        sourceFile.load(r, "func(a, , b, , c);");
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
        REQUIRE(callExpr->arguments.size() == 5);
    }
}

TEST_CASE("表达式 - 复杂表达式组合") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("混合表达式") {
        sourceFile.load(r, "a + b * c - d / e;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("带括号的表达式") {
        sourceFile.load(r, "(a + b) * (c - d);");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }

    SECTION("逻辑表达式组合") {
        sourceFile.load(r, "a > 0 && b < 10 || c == 5;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());
    }
}