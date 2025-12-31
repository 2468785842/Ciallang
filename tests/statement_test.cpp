/*
 * Copyright (c) 2024/12/18
 *
 * 语句单元测试
 */

#include <catch.hpp>

#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"
#include "parser/ast/AstBuilder.hpp"
#include "parser/ast/DeclNode.hpp"
#include "parser/ast/ExprNode.hpp"
#include "parser/ast/StmtNode.hpp"

using namespace Cial;

TEST_CASE("语句 - 变量声明语句") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("带初始化的变量声明") {
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
        REQUIRE(dynamic_cast<const Syntax::ValueExprNode *>(varDecl->rhs) != nullptr);
    }

    SECTION("不带初始化的变量声明") {
        sourceFile.load(r, "var x;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *varDecl = dynamic_cast<Syntax::VarDeclNode *>(blockNode->childrens[0]);
        REQUIRE(varDecl != nullptr);
        REQUIRE(varDecl->rhs == nullptr);
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
            auto *varDecl = dynamic_cast<Syntax::VarDeclNode *>(decl);
            REQUIRE(varDecl != nullptr);
        }
    }
}

TEST_CASE("语句 - 表达式语句") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("函数调用语句") {
        sourceFile.load(r, "print(\"hello\");");
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
        REQUIRE(dynamic_cast<const Syntax::ProcCallExprNode *>(exprStmt->expression) != nullptr);
    }

    SECTION("赋值语句") {
        sourceFile.load(r, "x = 10;");
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
        REQUIRE(dynamic_cast<const Syntax::AssignExprNode *>(exprStmt->expression) != nullptr);
    }
}

TEST_CASE("语句 - 代码块语句") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("空代码块") {
        sourceFile.load(r, "{}");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *blockStmt = dynamic_cast<const Syntax::BlockStmtNode *>(stmtDecl->statement);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.empty());
    }

    SECTION("带语句的代码块") {
        sourceFile.load(r, "{ var x = 1; x = x + 1; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *blockStmt = dynamic_cast<const Syntax::BlockStmtNode *>(stmtDecl->statement);
        REQUIRE(blockStmt != nullptr);
        REQUIRE(blockStmt->childrens.size() == 2);
    }

    SECTION("嵌套代码块") {
        sourceFile.load(r, "{ { var x = 1; } { var y = 2; } }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *outerBlock = dynamic_cast<const Syntax::BlockStmtNode *>(stmtDecl->statement);
        REQUIRE(outerBlock != nullptr);
        REQUIRE(outerBlock->childrens.size() == 2);

        for(auto *stmt : outerBlock->childrens) {
            auto *innerStmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(stmt);
            REQUIRE(innerStmtDecl != nullptr);
            auto *innerBlock = dynamic_cast<const Syntax::BlockStmtNode *>(innerStmtDecl->statement);
            REQUIRE(innerBlock != nullptr);
            REQUIRE(innerBlock->childrens.size() == 1);
        }
    }
}

TEST_CASE("语句 - 条件语句") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("简单if语句") {
        sourceFile.load(r, "if (x > 0) x = 1;");
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
        REQUIRE(ifStmt->elseBody == nullptr);
    }

    SECTION("if-else语句") {
        sourceFile.load(r, "if (x > 0) x = 1; else x = -1;");
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

    SECTION("if-else if-else语句") {
        sourceFile.load(r, "if (x > 0) x = 1; else if (x < 0) x = -1; else x = 0;");
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

        // 检查else分支中的if语句
        auto *elseBlock = dynamic_cast<Syntax::BlockStmtNode *>(ifStmt->elseBody);
        REQUIRE(elseBlock != nullptr);
        REQUIRE(elseBlock->childrens.size() == 1);
        auto *elseStmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(elseBlock->childrens[0]);
        REQUIRE(elseStmtDecl != nullptr);
        auto *elseIfStmt = dynamic_cast<const Syntax::IfStmtNode *>(elseStmtDecl->statement);
        REQUIRE(elseIfStmt != nullptr);
    }
}

TEST_CASE("语句 - 循环语句") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("while循环") {
        sourceFile.load(r, "while (i < 10) i = i + 1;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *whileStmt = dynamic_cast<const Syntax::WhileStmtNode *>(stmtDecl->statement);
        REQUIRE(whileStmt != nullptr);
        REQUIRE(whileStmt->test != nullptr);
        REQUIRE(whileStmt->body != nullptr);
    }

    SECTION("带代码块的while循环") {
        sourceFile.load(r, "while (true) { break; continue; }");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *whileStmt = dynamic_cast<const Syntax::WhileStmtNode *>(stmtDecl->statement);
        REQUIRE(whileStmt != nullptr);
        REQUIRE(dynamic_cast<const Syntax::BlockStmtNode *>(whileStmt->body) != nullptr);
    }
}

TEST_CASE("语句 - 控制转移语句") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("break语句") {
        sourceFile.load(r, "break;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *breakStmt = dynamic_cast<const Syntax::BreakStmtNode *>(stmtDecl->statement);
        REQUIRE(breakStmt != nullptr);
    }

    SECTION("continue语句") {
        sourceFile.load(r, "continue;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *continueStmt = dynamic_cast<const Syntax::ContinueStmtNode *>(stmtDecl->statement);
        REQUIRE(continueStmt != nullptr);
    }

    SECTION("return语句") {
        sourceFile.load(r, "return 42;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *returnStmt = dynamic_cast<const Syntax::ReturnStmtNode *>(stmtDecl->statement);
        REQUIRE(returnStmt != nullptr);
        REQUIRE(returnStmt->expr != nullptr);
    }

    SECTION("无返回值return语句") {
        sourceFile.load(r, "return;");
        Syntax::Parser parser{ sourceFile, astBuilder };

        auto *globalNode = parser.parse(r);
        REQUIRE(globalNode != nullptr);
        REQUIRE_FALSE(r.isFailed());

        auto *blockNode = dynamic_cast<Syntax::BlockStmtNode *>(globalNode);
        REQUIRE(blockNode != nullptr);
        REQUIRE(blockNode->childrens.size() == 1);
        auto *stmtDecl = dynamic_cast<Syntax::StmtDeclNode *>(blockNode->childrens[0]);
        REQUIRE(stmtDecl != nullptr);
        auto *returnStmt = dynamic_cast<const Syntax::ReturnStmtNode *>(stmtDecl->statement);
        REQUIRE(returnStmt != nullptr);
        REQUIRE(returnStmt->expr == nullptr);
    }
}

TEST_CASE("语句 - 复杂语句组合") {
    Common::SourceFile sourceFile{};
    Common::Result r{};
    Syntax::AstBuilder astBuilder{};

    SECTION("嵌套控制流") {
        sourceFile.load(r, "if (x > 0) { while (y < 10) { if (z == 5) break; y = y + 1; } }");
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
    }
}