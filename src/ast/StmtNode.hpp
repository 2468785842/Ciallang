// Copyright (c) 2024/5/26 下午5:02
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

#include "AstNode.hpp"

namespace Ciallang::Syntax {
    class StmtNode : public DeclNode {
    };

    class BlockStmtNode : public StmtNode {
    public:
        DeclNodeList childrens;

        BlockStmtNode() = delete;

        explicit BlockStmtNode(const char* name) : _name(name) {
        }

        void accept(const Visitor* visitor) const override {
            visitor->visit(this);
        }

        [[nodiscard]] const char* name() const noexcept override {
            return _name;
        }

    private:
        const char* _name;
    };

    class ExprStmtNode : public StmtNode {
    public:
        ExprNodeList expressions;

        ExprStmtNode() = delete;

        explicit ExprStmtNode(ExprNode* expr) {
            expressions.push_back(expr);
        }

        void accept(const Visitor* visitor) const override {
            visitor->visit(this);
        }

        [[nodiscard]] const char* name() const noexcept override {
            return "expression_statement";
        }
    };

    class IfStmtNode : public StmtNode {
    public:
        const ExprNode* test;
        const StmtNode* body;
        StmtNode* elseBody{ nullptr };

        IfStmtNode() = delete;

        explicit IfStmtNode(
            const ExprNode* test,
            const StmtNode* body
        ) : test(test), body(body) {
        }

        void accept(const Visitor* visitor) const override {
            visitor->visit(this);
        }

        [[nodiscard]] const char* name() const noexcept override {
            return "if_statement";
        }
    };
}
