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

#include "gen/BytecodeGen.hpp"

namespace Ciallang::Syntax {
    class StmtNode : public AstNode {
    protected:
        using AstNode::AstNode;
    };

    class BlockStmtNode final : public StmtNode {
    public:
        DeclNodeList childrens;

        explicit BlockStmtNode() : StmtNode("block_statement") {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        std::optional<Bytecode::Register> generateBytecode(Inter::BytecodeGen* gen) const override {
            return gen->generate(this);
        }
    };

    class ExprStmtNode final : public StmtNode {
    public:
        const ExprNode* expression;

        ExprStmtNode() = delete;

        explicit ExprStmtNode(const ExprNode* expr):
            StmtNode("expression_statement"),
            expression(expr) {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        std::optional<Bytecode::Register> generateBytecode(Inter::BytecodeGen* gen) const override {
            return gen->generate(this);
        }
    };

    class IfStmtNode final : public StmtNode {
    public:
        const ExprNode* test;
        const BlockStmtNode* body;
        BlockStmtNode* elseBody{ nullptr };

        IfStmtNode() = delete;

        explicit IfStmtNode(
            const ExprNode* test,
            const BlockStmtNode* body
        ) : StmtNode("if_statement"), test(test), body(body) {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        std::optional<Bytecode::Register> generateBytecode(Inter::BytecodeGen* gen) const override {
            return gen->generate(this);
        }
    };

    class WhileStmtNode final : public StmtNode {
    public:
        const ExprNode* test;
        const BlockStmtNode* body;

        WhileStmtNode() = delete;

        explicit WhileStmtNode(
            const ExprNode* test,
            const BlockStmtNode* body
        ): StmtNode("while_statement"), test(test), body(body) {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        std::optional<Bytecode::Register> generateBytecode(Inter::BytecodeGen* gen) const override {
            return gen->generate(this);
        }
    };

    class BreakStmtNode final : public StmtNode {
    public:
        BreakStmtNode() : StmtNode("break_statement") {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        std::optional<Bytecode::Register> generateBytecode(Inter::BytecodeGen* gen) const override {
            return gen->generate(this);
        }
    };

    class ContinueStmtNode final : public StmtNode {
    public:
        ContinueStmtNode() : StmtNode("continue_statement") {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        std::optional<Bytecode::Register> generateBytecode(Inter::BytecodeGen* gen) const override {
            return gen->generate(this);
        }
    };

    class ReturnStmtNode final : public StmtNode {
    public:
        const ExprNode* expr;

        explicit ReturnStmtNode(const ExprNode* expr) :
            StmtNode("return_statement"), expr(expr) {
        }

        void accept(Visitor* visitor) const override {
            visitor->visit(this);
        }

        std::optional<Bytecode::Register> generateBytecode(Inter::BytecodeGen* gen) const override {
            return gen->generate(this);
        }
    };
}
