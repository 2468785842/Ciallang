// Copyright (c) 2024/5/26 上午9:57
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

#pragma once

#include "../lexer/Token.hpp"
#include "AstNode.hpp"

namespace Ciallang::Syntax {
    class ExprNode : public AstNode {
    protected:
        using AstNode::AstNode;
    };

    class ValueExprNode final : public ExprNode {
    public:
        ValueExprNode() = delete;

        explicit ValueExprNode(Token& token) : ExprNode(token, "value") {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }
    };

    class SymbolExprNode final : public ExprNode {
    public:
        SymbolExprNode() = delete;

        explicit SymbolExprNode(Token& token) : ExprNode(token, "symbol") {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }
    };

    class BinaryExprNode final : public ExprNode {
    public:
        const ExprNode* lhs;
        const ExprNode* rhs;

        BinaryExprNode() = delete;

        explicit BinaryExprNode(
            Token& token,
            const ExprNode* lhs,
            const ExprNode* rhs
        ) : ExprNode(token, "binary_operator"), lhs(lhs), rhs(rhs) {
            location.start(lhs->location.start());
            location.end(rhs->location.end());
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }
    };

    class ProcCallExprNode final : public ExprNode {
    public:
        const ExprNode* memberAccess;
        std::vector<ExprNode*> arguments{};

        explicit ProcCallExprNode(
            const ExprNode* memberAccess
        ): ExprNode("call"), memberAccess(memberAccess) {
            location = memberAccess->location;
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }
    };

    class UnaryExprNode final : public ExprNode {
    public:
        const ExprNode* rhs;

        UnaryExprNode() = delete;

        explicit UnaryExprNode(
            Token& token,
            const ExprNode* rhs
        ) : ExprNode(token, "unary_operator"), rhs(rhs) {
            location.end(rhs->location.end());
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }
    };

    class AssignExprNode final : public ExprNode {
    public:
        const SymbolExprNode* lhs;
        const ExprNode* rhs;

        AssignExprNode() = delete;

        explicit AssignExprNode(
            const SymbolExprNode* lhs,
            const ExprNode* rhs
        ) : ExprNode("assignment_expression"), lhs(lhs), rhs(rhs) {
            location.start(lhs->location.start());
            location.end(rhs->location.end());
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }
    };
}
