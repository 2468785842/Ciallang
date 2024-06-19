/*
 * Copyright (c) 2024/6/14 下午9:51
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */
#pragma once

#include "pch.h"

#include "lexer/Token.hpp"
#include "AstNode.hpp"
#include "DeclNode.hpp"
#include "ExprNode.hpp"
#include "StmtNode.hpp"

namespace Ciallang {
    class AstFormatter final : public Syntax::AstNode::Visitor {
    public:
        explicit AstFormatter(const size_t initialIndent = 2) : leftPadding(initialIndent) {
        }

        void formatAst(const Syntax::AstNode* node) {
            node->accept(this);
        }

        template <typename T = Syntax::AstNode>
        void printNode(const std::string& type, const T* value = nullptr) {
            fmt::print("{0: <{2}}{1}", "", type, leftPadding);
            if(value) {
                if constexpr(std::is_base_of_v<Syntax::AstNode, T>) {
                    fmt::print("(");
                    value->accept(this);
                    fmt::print(")");
                } else if constexpr(std::is_same_v<TjsValue, T>) {
                    fmt::print(" ({})", *value);
                } else if constexpr(std::is_same_v<char, T>) {
                    fmt::print(" ({})", value);
                } else {
                    static_assert(false, "not support");
                }
            }
            fmt::println("{}", "");
        }

        void visit(const Syntax::StmtDeclNode* node) override {
            node->statement->accept(this);
        }

        void visit(const Syntax::VarDeclNode* node) override {
            printNode("DeclareVar");
            increaseIndent();

            CHECK(node->token->value()->isString());
            printNode("Variable", node->token->value());

            printNode("=");
            node->rhs->accept(this);

            decreaseIndent();
        }

        void visit(const Syntax::FunctionDeclNode* node) override {
            printNode("FunctionDecl");
            increaseIndent();

            printNode("(Parameters)");

            increaseIndent();
            for(auto& [token,exprNode] : node->parameters) {
                CHECK(token.value()->isString());
                printNode(*token.value()->asString(), exprNode);
            }
            decreaseIndent();

            printNode("(Body)");
            increaseIndent();
            node->body->accept(this);
            decreaseIndent();

            decreaseIndent();
        }

        void visit(const Syntax::ValueExprNode* node) override {
            printNode("Value", node->token->value());
        }

        void visit(const Syntax::IdentifierExprNode* node) override {
            CHECK(node->token->value()->isString());
            printNode("Identifier", node->token->value());
        }

        void visit(const Syntax::BinaryExprNode* node) override {
            printNode("BinaryExpression");
            increaseIndent();

            node->lhs->accept(this);
            printNode("Operator", node->token->name());
            node->rhs->accept(this);

            decreaseIndent();
        }

        void visit(const Syntax::UnaryExprNode* node) override {
            printNode("UnaryExpression");
            increaseIndent();

            printNode("Operator", node->token->name());
            node->rhs->accept(this);

            decreaseIndent();
        }

        void visit(const Syntax::ProcCallExprNode* node) override {
            printNode("ProcCall");
            increaseIndent();

            printNode("(Arguments)");

            increaseIndent();
            for(auto argument : node->arguments) {
                argument->accept(this);
            }
            decreaseIndent();

            decreaseIndent();
        }

        void visit(const Syntax::AssignExprNode* node) override {
            printNode("AssignmentExpression");
            increaseIndent();

            node->lhs->accept(this);
            printNode("=");
            node->rhs->accept(this);

            decreaseIndent();
        }

        void visit(const Syntax::BlockStmtNode* node) override {
            printNode("BlockStatement");
            increaseIndent();

            printNode("(Children)");

            increaseIndent();
            for(auto* child : node->childrens) {
                child->accept(this);
            }
            decreaseIndent();

            decreaseIndent();
        }

        void visit(const Syntax::ExprStmtNode* node) override {
            node->expression->accept(this);
        }

        void visit(const Syntax::IfStmtNode* node) override {
            printNode("IfStatement");
            increaseIndent();

            printNode("(Condition)");

            increaseIndent();
            node->test->accept(this);
            decreaseIndent();

            printNode("(Body)");

            increaseIndent();
            node->body->accept(this);
            decreaseIndent();

            if(node->elseBody) {
                printNode("(ElseBody)");
                increaseIndent();
                node->elseBody->accept(this);
                decreaseIndent();
            }

            decreaseIndent();
        }

        void visit(const Syntax::WhileStmtNode* node) override {
            printNode("WhileStatement");
            increaseIndent();

            printNode("(Condition)");
            increaseIndent();
            node->test->accept(this);
            decreaseIndent();

            printNode("(Body)");
            increaseIndent();
            node->body->accept(this);
            decreaseIndent();

            decreaseIndent();
        }

        void visit(const Syntax::BreakStmtNode* node) override {
            printNode("BreakStatement");
        }

        void visit(const Syntax::ContinueStmtNode* node) override {
            printNode("ContinueStatement");
        }

        void visit(const Syntax::ReturnStmtNode* node) override {
            printNode("ReturnStatement");
            increaseIndent();
            node->expr->accept(this);

            decreaseIndent();
        }

        void increaseIndent() { leftPadding += 2; }
        void decreaseIndent() { leftPadding = std::max(leftPadding - 2, static_cast<size_t>(0)); }

    private:
        size_t leftPadding{ 0 };
    };
}
