/*
 * Copyright (c) 2024/5/6 下午8:16
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

#include "lexer/Token.hpp"

#include "AstNode.hpp"

namespace Ciallang::Syntax {
    class AstBuilder {
        std::vector<AstNode*> _nodes{};

    public:
        AstBuilder() = default;

        ~AstBuilder();

        ExprStmtNode* makeExprStmtNode(const ExprNode* rhs);

        IfStmtNode* makeIfStmtNode(const ExprNode* test, const BlockStmtNode* body);

        WhileStmtNode* makeWhileStmtNode(const ExprNode* test, const BlockStmtNode* body);

        ValueExprNode* makeValueExprNode(Token&& op);

        BinaryExprNode* makeBinaryExprNode(Token&& op, const ExprNode* lhs,
                                           const ExprNode* rhs);

        UnaryExprNode* makeUnaryExprNode(Token&& op, const ExprNode* rhs);

        ProcCallExprNode* makeProcCallExprNode(const ExprNode* lhs);

        AssignExprNode* makeAssignExprNode(const IdentifierExprNode* lhs,
                                           const ExprNode* rhs);

        IdentifierExprNode* makeSymbolExprNode(Token&& token);

        FunctionDeclNode* makeFunctionDeclNode(Token&& token);

        StmtDeclNode* makeStmtDeclNode(const StmtNode* stmtNode);

        BreakStmtNode* makeBreakStmtNode();

        ContinueStmtNode* makeContinueStmtNode();

        ReturnStmtNode* makeReturnStmtNode(const ExprNode* node);

        template <typename R, typename... Args>
        R* makeNode(Args&&... args) {
            static_assert(
                std::is_base_of_v<AstNode, R>,
                "Error: R must be a derived class of AstNode."
            );

            R* node = new R{ std::forward<Args>(args)... };
            _nodes.push_back(node);
            return node;
        }
    };
}
