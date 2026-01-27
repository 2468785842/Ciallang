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
#include "StmtNode.hpp"

#include "gen/IRGenerator.hpp"

namespace cial::syntax {
    class DeclNode : public AstNode {
    protected:
        using AstNode::AstNode;
    };

    class VarDeclNode final : public DeclNode {
    public:
        const ExprNode *rhs;
        const VarDeclNode *next;

        VarDeclNode() = delete;

        explicit VarDeclNode(const Token &token, const ExprNode *rhs, const VarDeclNode *next) :
            DeclNode(token, "var_declaration"), rhs(rhs), next(next) {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class FunctionDeclNode final : public DeclNode {
    public:
        BlockStmtNode *body{ nullptr };
        // name, default value
        Parameters parameters{};

        FunctionDeclNode() = delete;

        explicit FunctionDeclNode(const Token &token) : DeclNode(token, "function_declaration") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class ClassDeclNode final : public DeclNode {
    public:
        Vec<IdentifierExprNode *> extends{};
        FunctionDeclNode *constructor{};

        Vec<VarDeclNode *> varDeclVec{};
        Vec<PropertyDeclNode *> propertyDeclVec{};
        Vec<FunctionDeclNode *> funcDeclVec{};

        ClassDeclNode() = delete;

        explicit ClassDeclNode(const Token &token) : DeclNode(token, "class_declaration") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class PropertyDeclNode final : public DeclNode {
    public:
        FunctionDeclNode *setter{ nullptr };
        FunctionDeclNode *getter{ nullptr };

        PropertyDeclNode() = delete;

        explicit PropertyDeclNode(const Token &token) : DeclNode(token, "property_declaration") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class StmtDeclNode final : public DeclNode {
    public:
        const StmtNode *statement;

        StmtDeclNode() = delete;

        explicit StmtDeclNode(const StmtNode *stmtNode) : DeclNode("statement_declaration"), statement(stmtNode) {
            this->location = statement->location;
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };
} // namespace cial::syntax
