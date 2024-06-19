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

#include "gen/BytecodeGen.hpp"

namespace Ciallang::Syntax {
    class DeclNode : public AstNode {
    protected:
        using AstNode::AstNode;
    };

    class VarDeclNode final : public DeclNode {
    public:
        const ExprNode* rhs;

        VarDeclNode() = delete;

        explicit VarDeclNode(
            Token& token, const ExprNode* rhs
        ) : DeclNode(token, "var_declaration"), rhs(rhs) {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        std::optional<Bytecode::Register> generateBytecode(Inter::BytecodeGen* gen) const override {
            return gen->generate(this);
        }
    };

    class FunctionDeclNode final : public DeclNode {
    public:
        BlockStmtNode* body{ nullptr };
        // name, default value
        std::vector<std::pair<Token, ExprNode*>> parameters{};

        FunctionDeclNode() = delete;

        explicit FunctionDeclNode(Token& token):
            DeclNode(token, "function_declaration") {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        std::optional<Bytecode::Register> generateBytecode(Inter::BytecodeGen* gen) const override {
            return gen->generate(this);
        }
    };

    class StmtDeclNode final : public DeclNode {
    public:
        const StmtNode* statement;

        StmtDeclNode() = delete;

        explicit StmtDeclNode(const StmtNode* stmtNode):
            DeclNode("statement_declaration"),
            statement(stmtNode) {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        std::optional<Bytecode::Register> generateBytecode(Inter::BytecodeGen* gen) const override {
            return gen->generate(this);
        }
    };
}
