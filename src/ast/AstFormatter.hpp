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

#include "AstNode.hpp"
#include "DeclNode.hpp"
#include "ExprNode.hpp"
#include "StmtNode.hpp"
#include "lexer/Token.hpp"
#include "logging/Logger.hpp"

namespace Ciallang {
    class AstFormatter final : public Syntax::AstNode::Visitor {
    public:
        explicit AstFormatter() = default;

        void formatAst(const Syntax::AstNode *node) {
            _branchStack.clear();
            // root
            _ss << "Program" << '\n';
            // print root node as a single child (no sibling)
            child(node, true);
            fmt::println("{}", _ss.str());
            _ss.clear();
        }

    private:
        // 用于树状绘制：vector 中每一层标志该层是否还有后续兄弟节点
        std::vector<bool> _branchStack;
        std::stringstream _ss;

        // ======= 前缀绘制 =======
        void printPrefix() {
            // 对 branchStack 的每一层（除了最后一层）输出 "│   " 或 "    "
            for(size_t i = 0; i + 1 < _branchStack.size(); ++i) {
                _ss << fmt::format("{}", _branchStack[i] ? "│   " : "    ");
            }
            // 最后一层使用 "├── "（还有兄弟）或 "└── "（已是最后）
            if(!_branchStack.empty()) {
                _ss << fmt::format("{}", _branchStack.back() ? "├── " : "└── ");
            }
        }

        // 保持和你原来签名接近的 printNode（T* 默认 nullptr）
        template <typename T = Syntax::AstNode>
        void printNode(const std::string &type, const T *value = nullptr) {
            printPrefix();
            _ss << fmt::format("{}", type);

            if(value) {
                if constexpr(std::is_base_of_v<Syntax::AstNode, T>) {
                    // 不在这里直接递归 accept（会导致前缀错位）
                    // 我们选择在 visitor 中明确 child(...) 来控制分支标志
                    _ss << fmt::format(" ");
                    // 如果有人直接调用 printNode(type, nodePtr)（兼容性），则把它当作单独子节点处理：
                    // 打印 "(" then the child subtree then ")"
                    _ss << fmt::format("(");
                    // 为了保证子树正确缩进，作为内嵌子节点，我们 push 一个 hasSibling=false
                    _branchStack.push_back(false);
                    value->accept(this);
                    _branchStack.pop_back();
                    _ss << fmt::format(")");
                } else if constexpr(std::is_same_v<char, T>) {
                    _ss << fmt::format(" ({})", value);
                } else {
                    static_assert(!std::is_same_v<T, T>, "not support");
                }
            }
            _ss << '\n';
        }

        // 原来有一个重载专门处理 Value
        void printNode(const std::string &type, const Value &value) {
            printPrefix();
            _ss << fmt::format("{} ({})\n", type, value);
        }

        // ======= child helpers: 用来统一管理 branchStack（避免直接把节点当成可调用对象） =======
        // 传 AstNode*，自动 accept(this)
        void child(const Syntax::AstNode *node, const bool hasSibling) {
            _branchStack.push_back(hasSibling);
            node->accept(this);
            _branchStack.pop_back();
        }

        // 传 lambda（可用于一次打印多个子节点或临时组合）
        void child(const std::function<void()> &fn, const bool hasSibling) {
            _branchStack.push_back(hasSibling);
            fn();
            _branchStack.pop_back();
        }

        // ======= VISITOR 实现（全部覆盖） =======

        void visit(const Syntax::StmtDeclNode *node) override {
            // StmtDecl 只是包装 statement
            node->statement->accept(this);
        }

        void visit(const Syntax::VarDeclNode *node) override {
            printNode("DeclareVar");
            child(
                [&] {
                    // Variable (name)
                    printNode("Variable", node->token->value());
                    // "="
                    printNode("=");
                    // rhs
                    node->rhs->accept(this);
                },
                false);
        }

        void visit(const Syntax::FunctionDeclNode *node) override {
            printNode("FunctionDecl");
            child(
                [&] {
                    // Parameters
                    printNode("(Parameters)");
                    child(
                        [&] {
                            for(size_t i = 0; i < node->parameters.size(); ++i) {
                                const auto &[token, exprNode] = node->parameters[i];
                                const bool hasSibling = i + 1 < node->parameters.size();
                                // 参数名称作为 label，参数表达式作为子节点（如果有）
                                // 若 exprNode 非空则把它作为 child，否则只打印名称
                                child(
                                    [&] {
                                        // 打成 "name" 然后把 exprNode 当成子节点（如果存在）
                                        printNode(token.value().toString()->toStdStr());
                                        if(exprNode) {
                                            // 将参数的 expr 作为该名称的子节点
                                            child(exprNode, false);
                                        }
                                    },
                                    hasSibling);
                            }
                        },
                        false);

                    // Body
                    printNode("(Body)");
                    child(node->body, false);
                },
                false);
        }

        void visit(const Syntax::ClassDeclNode *node) override {
            printNode("ClassDecl");
            child(
                [&] {
                    // Parameters / Members
                    printNode("(Parameters)");
                    child(
                        [&] {
                            for(size_t i = 0; i < node->body->childrens.size(); ++i) {
                                const bool hasSibling = i + 1 < node->body->childrens.size();
                                child(node->body->childrens[i], hasSibling);
                            }
                        },
                        true);

                    // Body
                    printNode("(Body)");
                    child(node->body, false);
                },
                false);
        }

        void visit(const Syntax::BinaryExprNode *node) override {
            printNode("BinaryExpression");
            child(
                [&] {
                    node->lhs->accept(this);
                    printNode("Operator", node->token->name());
                    node->rhs->accept(this);
                },
                false);
        }

        void visit(const Syntax::UnaryExprNode *node) override {
            printNode("UnaryExpression");
            child(
                [&] {
                    printNode("Operator", node->token->name());
                    node->rhs->accept(this);
                },
                false);
        }

        void visit(const Syntax::ProcCallExprNode *node) override {
            printNode("ProcCall");
            if(!node->arguments.empty()) {
                child(
                    [&] {
                        printNode("(Arguments)");
                        for(size_t i = 0; i < node->arguments.size(); ++i) {
                            const bool hasSibling = i + 1 < node->arguments.size();
                            child(node->arguments[i], hasSibling);
                        }
                    },
                    false);
            }
        }

        void visit(const Syntax::AssignExprNode *node) override {
            printNode("AssignmentExpression");
            child(
                [&] {
                    node->lhs->accept(this);
                    printNode("=");
                    node->rhs->accept(this);
                },
                false);
        }

        void visit(const Syntax::BlockStmtNode *node) override {
            printNode("BlockStatement");
            child(
                [&] {
                    printNode("(Children)");
                    for(size_t i = 0; i < node->childrens.size(); ++i) {
                        const bool hasSibling = i + 1 < node->childrens.size();
                        child(node->childrens[i], hasSibling);
                    }
                },
                false);
        }

        void visit(const Syntax::ExprStmtNode *node) override { node->expression->accept(this); }

        void visit(const Syntax::IfStmtNode *node) override {
            printNode("IfStatement");
            child(
                [&] {
                    printNode("(Condition)");
                    child(node->test, false);

                    printNode("(Body)");
                    child(node->body, node->elseBody != nullptr);

                    if(node->elseBody) {
                        printNode("(ElseBody)");
                        child(node->elseBody, false);
                    }
                },
                false);
        }

        void visit(const Syntax::DoWhileStmtNode *node) override {
            printNode("DoWhileStatement");
            child(
                [&] {
                    printNode("(Body)");
                    child(node->body, true);

                    printNode("(Condition)");
                    child(node->test, false);
                },
                false);
        }

        void visit(const Syntax::ForStmtNode *node) override {
            printNode("ForStatement");
            child(
                [&] {
                    if(node->init) {
                        printNode("(Init)");
                        child(node->init, true);
                    }

                    if(node->test) {
                        printNode("(Condition)");
                        child(node->test, true);
                    }

                    if(node->step) {
                        printNode("(Step)");
                        child(node->step, true);
                    }

                    printNode("(Body)");
                    child(node->body, false);
                },
                false);
        }

        void visit(const Syntax::WhileStmtNode *node) override {
            printNode("WhileStatement");
            child(
                [&] {
                    printNode("(Condition)");
                    child(node->test, true);

                    printNode("(Body)");
                    child(node->body, false);
                },
                false);
        }

        void visit(const Syntax::BreakStmtNode *node) override { printNode("BreakStatement"); }

        void visit(const Syntax::ContinueStmtNode *node) override { printNode("ContinueStatement"); }

        void visit(const Syntax::ReturnStmtNode *node) override {
            printNode("ReturnStatement");
            child([&] { node->expr->accept(this); }, false);
        }

        void visit(const Syntax::IdentifierExprNode *node) override {
            printPrefix();
            // 打成 Identifier (name)
            if(node->token && node->token->value().isString()) {
                _ss << fmt::format("Identifier ({})", node->token->value()) << '\n';
            } else {
                _ss << fmt::format("Identifier") << '\n';
            }
        }

        void visit(const Syntax::ValueExprNode *node) override {
            printPrefix();
            _ss << fmt::format("Value ({})", node->token->value()) << '\n';
        }
    };

} // namespace Ciallang
