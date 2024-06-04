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

#include "../lexer/Token.hpp"

#include "DeclNode.hpp"
#include "StmtNode.hpp"
#include "ExprNode.hpp"
#include "AstNode.hpp"

namespace Ciallang::Syntax {
    class AstBuilder {
        std::stack<BlockStmtNode*> _scopeStack{};
        std::forward_list<AstNode*> _nodes{};

    public:
        AstBuilder() = default;

        ~AstBuilder();

        void reset();

        // scope/block stack
        BlockStmtNode* endScope();

        BlockStmtNode* popScope();

        BlockStmtNode* beginScope();

        void pushScope(BlockStmtNode* node);

        BlockStmtNode* globalNode();

        BlockStmtNode* basicBlockNode();

        ExprStmtNode* makeExprStmtNode(ExprNode* rhs);

        IfStmtNode* makeIfStmtNode(const ExprNode* test, const StmtNode* body);

        ValueExprNode* makeValueExprNode(Token&& op);

        BinaryExprNode* makeBinaryExprNode(Token&& op, const ExprNode* lhs,
                                           const ExprNode* rhs);

        UnaryExprNode* makeUnaryExprNode(Token&& op, const ExprNode* rhs);

        AssignExprNode* makeAssignExprNode(const SymbolExprNode* lhs,
                                           const ExprNode* rhs);

        SymbolExprNode* makeSymbolExprNode(Token&& token);

        VarDeclNode* makeVarDeclNode(const ExprStmtNode* rhs);

        AstNode* pairNode();

        AstNode* procCallNode();

        // 实参list
        AstNode* argumentListNode();

        AstNode* functionNode();

        template <typename R, typename... Args>
        R* makeNode(Args&&... args);
    };
}
