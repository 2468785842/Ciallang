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

#include "AstNode.hpp"

#include "gen/IRGenerator.hpp"
#include "parser/Token.hpp"

namespace cial::syntax {
    class ExprNode : public AstNode {
    protected:
        using AstNode::AstNode;
    };

    class ValueExprNode final : public ExprNode {
    public:
        ValueExprNode() = delete;

        explicit ValueExprNode(const Token &token) : ExprNode(token, "value") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class IdentifierExprNode final : public ExprNode {
    public:
        IdentifierExprNode() = delete;

        explicit IdentifierExprNode(const Token &token) : ExprNode(token, "identifier") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class InternalIdentifierExprNode final : public ExprNode {
    public:
        InternalIdentifierExprNode() = delete;

        explicit InternalIdentifierExprNode(const Token &token) : ExprNode(token, "internal_identifier") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class BinaryExprNode final : public ExprNode {
    public:
        const ExprNode *lhs;
        const ExprNode *rhs;

        BinaryExprNode() = delete;

        /**
         * 创建一个二目运算节点
         *
         * @param token 根节点
         * @param lhs 左叶子节点
         * @param rhs 右叶子节点
         * @return 二目运算符AstNode
         */
        explicit BinaryExprNode(const Token &token, const ExprNode *lhs, const ExprNode *rhs) :
            ExprNode(token, "binary_operator"), lhs(lhs), rhs(rhs) {
            location.start(lhs->location.start());
            location.end(rhs->location.end());
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class ProcCallExprNode final : public ExprNode {
    public:
        const ExprNode *memberAccess;
        Vec<ExprNode *> arguments{};

        explicit ProcCallExprNode(const ExprNode *memberAccess) : ExprNode("call"), memberAccess(memberAccess) {
            location = memberAccess->location;
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class PrefixUnaryExprNode final : public ExprNode {
    public:
        const ExprNode *rhs;

        PrefixUnaryExprNode() = delete;

        explicit PrefixUnaryExprNode(const Token &token, const ExprNode *rhs) :
            ExprNode(token, "prefix_unary_operator"), rhs(rhs) {
            location.end(rhs->location.end());
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class SuffixUnaryExprNode final : public ExprNode {
    public:
        const ExprNode *lhs;

        SuffixUnaryExprNode() = delete;

        explicit SuffixUnaryExprNode(const Token &token, const ExprNode *lhs) :
            ExprNode(token, "suffix_unary_operator"), lhs(lhs) {
            location.end(lhs->location.end());
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class AssignExprNode final : public ExprNode {
    public:
        const ExprNode *lhs;
        const ExprNode *rhs;

        AssignExprNode() = delete;

        explicit AssignExprNode(const ExprNode *lhs, const ExprNode *rhs) :
            ExprNode("assignment_expression"), lhs(lhs), rhs(rhs) {
            location.start(lhs->location.start());
            location.end(rhs->location.end());
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class FunctionExprNode final : public ExprNode {
    public:
        BlockStmtNode *body{ nullptr };
        // name, default value
        Vec<std::pair<Token, ExprNode *>> parameters{};

        explicit FunctionExprNode() : ExprNode("function_declaration") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    class TernaryExprNode final : public ExprNode {
    public:
        const ExprNode *test;
        const ExprNode *lhsExpr;
        const ExprNode *rhsExpr;

        explicit TernaryExprNode(const ExprNode *test, const ExprNode *lhsExpr, const ExprNode *rhsExpr) :
            ExprNode("conditional_ternary"), test(test), lhsExpr(lhsExpr), rhsExpr(rhsExpr) {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    // a[index] 下标访问（数组/字典/字符串/对象成员通读）；写入由 Assign lhs 分支处理
    class IndexExprNode final : public ExprNode {
    public:
        const ExprNode *lhs;
        const ExprNode *index;

        IndexExprNode() = delete;

        explicit IndexExprNode(const ExprNode *lhs, const ExprNode *index) :
            ExprNode("index_access"), lhs(lhs), index(index) {
            location = lhs->location;
            location.end(index->location.end());
        }

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    // [e1, e2, ...] 数组字面量（每次求值新建 Array）
    class ArrayLiteralNode final : public ExprNode {
    public:
        Vec<ExprNode *> elements{};

        explicit ArrayLiteralNode() : ExprNode("array_literal") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    // %["k" => v, sym : v2, ...] 字典字面量（每次求值新建 Dictionary）
    class DictLiteralNode final : public ExprNode {
    public:
        Vec<std::pair<ExprNode *, ExprNode *>> pairs{};

        explicit DictLiteralNode() : ExprNode("dict_literal") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };

    // 裸词链基座 .member 的隐式对象（with 语句的上下文对象；无 with 时为 global），
    // 生成 GetWith 指令；后续 .b.c 由普通 Dot 中缀续链
    class WithBaseExprNode final : public ExprNode {
    public:
        explicit WithBaseExprNode(const Token &token) : ExprNode(token, "with_base") {}

        void accept(Visitor *visitor) const override { visitor->visit(this); }

        void generateBytecode(inter::IRGenerator *gen, OptReg &retReg) const override {
            return gen->generate(this, retReg);
        }
    };
} // namespace cial::syntax
