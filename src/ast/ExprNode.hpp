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
    public:
        const Token* token{ nullptr };

        explicit ExprNode() = default;

        explicit ExprNode(Token& token) : token(new Token{ std::move(token) }) {
            location = this->token->location;
        }

        ~ExprNode() override { delete token; }
    };

    class ValueExprNode : public ExprNode {
    public:
        ValueExprNode() = delete;

        explicit ValueExprNode(Token& token) : ExprNode(token) {
        }

        void accept(const Visitor* visitor) const override {
            visitor->visit(this);
        }

        [[nodiscard]] const char* name() const noexcept override {
            return "value";
        }
    };

    class SymbolExprNode : public ExprNode {
    public:
        SymbolExprNode() = delete;

        explicit SymbolExprNode(Token& token) : ExprNode(token) {
        }

        void accept(const Visitor* visitor) const override {
            visitor->visit(this);
        }

        [[nodiscard]] const char* name() const noexcept override {
            return "symbol";
        }
    };

    class BinaryExprNode : public ExprNode {
    public:
        const ExprNode* lhs;
        const ExprNode* rhs;

        BinaryExprNode() = delete;

        explicit BinaryExprNode(
            Token& token,
            const ExprNode* lhs,
            const ExprNode* rhs
        ) : ExprNode(token), lhs(lhs), rhs(rhs) {
            location.start(lhs->location.start());
            location.end(rhs->location.end());
        }

        void accept(const Visitor* visitor) const override {
            visitor->visit(this);
        }

        [[nodiscard]] const char* name() const noexcept override {
            return "binary_operator";
        }
    };

    class UnaryExprNode : public ExprNode {
    public:
        const ExprNode* rhs;

        UnaryExprNode() = delete;

        explicit UnaryExprNode(
            Token& token,
            const ExprNode* rhs
        ) : ExprNode(token), rhs(rhs) {
            location.end(rhs->location.end());
        }

        void accept(const Visitor* visitor) const override {
            visitor->visit(this);
        }

        [[nodiscard]] const char* name() const noexcept override {
            return "unary_operator";
        }
    };

    class AssignExprNode : public ExprNode {
    public:
        const SymbolExprNode* lhs;
        const ExprNode* rhs;

        AssignExprNode() = delete;

        explicit AssignExprNode(
            const SymbolExprNode* lhs,
            const ExprNode* rhs
        ) : lhs(lhs), rhs(rhs) {
            location.start(lhs->location.start());
            location.end(rhs->location.end());
        }

        void accept(const Visitor* visitor) const override {
            visitor->visit(this);
        }

        [[nodiscard]] const char* name() const noexcept override {
            return "assignment_expression";
        }
    };
}
