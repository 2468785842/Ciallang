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
#include "ExprNode.hpp"

#include "gen/IRGenerator.hpp"

namespace Cial::Syntax {
    class StmtNode : public AstNode {
    protected:
        using AstNode::AstNode;
    };

    class BlockStmtNode final : public StmtNode {
    public:
        std::vector<DeclNode *> childrens;

        explicit BlockStmtNode() : StmtNode("block_statement") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(Inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class ExprStmtNode final : public StmtNode {
    public:
        const ExprNode *expression;

        ExprStmtNode() = delete;

        explicit ExprStmtNode(const ExprNode *expr) : StmtNode("expression_statement"), expression(expr) {
            this->location = expression->location;
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(Inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class IfStmtNode final : public StmtNode {
    public:
        const ExprNode *test;
        const BlockStmtNode *body;
        BlockStmtNode *elseBody{ nullptr };

        IfStmtNode() = delete;

        explicit IfStmtNode(const ExprNode *test, const BlockStmtNode *body) :
            StmtNode("if_statement"), test(test), body(body) {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(Inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };


    class DoWhileStmtNode final : public StmtNode {
    public:
        const BlockStmtNode *body;
        const ExprNode *test;

        DoWhileStmtNode() = delete;

        explicit DoWhileStmtNode(const BlockStmtNode *body, const ExprNode *test) :
            StmtNode("do_while_statement"), body(body), test(test) {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(Inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class ForStmtNode final : public StmtNode {
    public:
        const DeclNode *init{ nullptr };
        const ExprNode *test{ nullptr };
        const ExprNode *step{ nullptr };
        const BlockStmtNode *body{ nullptr };

        ForStmtNode() = delete;

        explicit ForStmtNode(const DeclNode *init, const ExprNode *test, const ExprNode *step,
                             const BlockStmtNode *body) :
            StmtNode("for_statement"), init(init), test(test), step(step), body(body) {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(Inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class WhileStmtNode final : public StmtNode {
    public:
        const ExprNode *test;
        const BlockStmtNode *body;

        WhileStmtNode() = delete;

        explicit WhileStmtNode(const ExprNode *test, const BlockStmtNode *body) :
            StmtNode("while_statement"), test(test), body(body) {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(Inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class BreakStmtNode final : public StmtNode {
    public:
        BreakStmtNode() : StmtNode("break_statement") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(Inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class ContinueStmtNode final : public StmtNode {
    public:
        ContinueStmtNode() : StmtNode("continue_statement") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(Inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class ReturnStmtNode final : public StmtNode {
    public:
        const ExprNode *expr;

        explicit ReturnStmtNode(const ExprNode *expr) : StmtNode("return_statement"), expr(expr) {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(Inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };
} // namespace Cial::Syntax
