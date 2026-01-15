// Copyright (c) 2024/5/26 下午5:00
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

#include "common/SourceLocation.hpp"
#include "parser/Token.hpp"
#include "vm/Register.hpp"

namespace cial::Inter {
    class IRGenerator;
}

namespace cial::Syntax {
    class AstNode;

    class DeclNode;

    class ExprNode;

    class StmtNode;

    class ValueExprNode;

    class IdentifierExprNode;

    class InternalIdentifierExprNode;

    class BinaryExprNode;

    class PrefixUnaryExprNode;

    class SuffixUnaryExprNode;

    class ProcCallExprNode;

    class AssignExprNode;

    class FunctionExprNode;

    class BlockStmtNode;

    class ExprStmtNode;

    class IfStmtNode;

    class SwitchStmtNode;

    class DoWhileStmtNode;

    class ForStmtNode;

    class WhileStmtNode;

    class BreakStmtNode;

    class ContinueStmtNode;

    class ReturnStmtNode;

    class TernaryExprNode;

    class PropertyDeclNode;

    class VarDeclNode;

    class FunctionDeclNode;

    class ClassDeclNode;

    class StmtDeclNode;

    using Parameters = std::vector<std::pair<Token, ExprNode *>>;

    class AstNode {
    protected:
        explicit AstNode(const char *name) : _name(name) {}

        explicit AstNode(Token token, const char *name) : token(std::move(token)), _name(name) {
            location = this->token.location;
        }

    public:
        struct Visitor;

        const Token token{};

        AstNode() = delete;

        SourceLocation location{};

        virtual void accept(Visitor *) const = 0;

        virtual void generateBytecode(Inter::IRGenerator *, OptReg &) const = 0;

        [[nodiscard]] std::string_view name() const noexcept { return _name; }

        virtual ~AstNode() = default;

    private:
        const std::string _name{};
    };

    struct AstNode::Visitor {
        virtual ~Visitor() = default;

        virtual void visit(const StmtDeclNode *) = 0;

        virtual void visit(const PropertyDeclNode *) = 0;

        virtual void visit(const VarDeclNode *) = 0;

        virtual void visit(const FunctionDeclNode *) = 0;

        virtual void visit(const ClassDeclNode *) = 0;

        virtual void visit(const ValueExprNode *) = 0;

        virtual void visit(const IdentifierExprNode *) = 0;

        virtual void visit(const InternalIdentifierExprNode *) = 0;

        virtual void visit(const BinaryExprNode *) = 0;

        virtual void visit(const PrefixUnaryExprNode *) = 0;

        virtual void visit(const SuffixUnaryExprNode *) = 0;

        virtual void visit(const ProcCallExprNode *) = 0;

        virtual void visit(const AssignExprNode *) = 0;

        virtual void visit(const FunctionExprNode *) = 0;

        virtual void visit(const BlockStmtNode *) = 0;

        virtual void visit(const ExprStmtNode *) = 0;

        virtual void visit(const IfStmtNode *) = 0;

        virtual void visit(const SwitchStmtNode *) = 0;

        virtual void visit(const DoWhileStmtNode *) = 0;

        virtual void visit(const ForStmtNode *) = 0;

        virtual void visit(const WhileStmtNode *) = 0;

        virtual void visit(const BreakStmtNode *) = 0;

        virtual void visit(const ContinueStmtNode *) = 0;

        virtual void visit(const ReturnStmtNode *) = 0;

        virtual void visit(const TernaryExprNode *) = 0;
    };
} // namespace cial::Syntax
