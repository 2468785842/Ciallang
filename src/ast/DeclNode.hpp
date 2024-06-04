/*
 * Copyright (c) 2024/6/3 下午4:47
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

namespace Ciallang::Syntax {
    class DeclNode : public AstNode {
        using AstNode::AstNode;
    };

    class VarDeclNode : public DeclNode {
    public:
        const ExprStmtNode* rhs;

        VarDeclNode() = delete;

        explicit VarDeclNode(
            Token& token, const ExprStmtNode* rhs
        ) : DeclNode(token), rhs(rhs) {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        [[nodiscard]] const char* name() const noexcept override {
            return "var_declaration";
        }
    };
}
