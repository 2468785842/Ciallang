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

#include "../common/SourceLocation.hpp"

namespace Ciallang::Syntax {
    class AstNode;

    class DeclNode;

    class ExprNode;

    class StmtNode;

    class ValueExprNode;

    class SymbolExprNode;

    class BinaryExprNode;

    class UnaryExprNode;

    class ProcCallExprNode;

    class AssignExprNode;

    class BlockStmtNode;

    class ExprStmtNode;

    class IfStmtNode;

    class WhileStmtNode;

    class BreakStmtNode;

    class ContinueStmtNode;

    class ReturnStmtNode;

    class VarDeclNode;

    class FunctionDeclNode;

    class StmtDeclNode;

    using DeclNodeList = std::vector<DeclNode*>;

    class AstNode {
    protected:
        explicit AstNode(std::string&& name) : _name(std::move(name)) {
        }

        explicit AstNode(
            Token& token, std::string&& name
        ) : token(new Token{ std::move(token) }), _name(std::move(name)) {
            location = this->token->location;
        }

    public:
        struct Visitor;

        const std::unique_ptr<Token> token{ nullptr };

        AstNode() = delete;

        // just for Gen Graphviz used
        const uint64_t id = serialId++;

        SourceLocation location{};

        virtual void accept(Visitor* visitor) const = 0;

        [[nodiscard]] std::string_view name() const noexcept {
            return _name;
        };

        virtual ~AstNode() = default;

    private:
        static inline uint64_t serialId = 0;
        const std::string _name{};
    };

    struct AstNode::Visitor {
        virtual ~Visitor() = default;

        virtual void visit(const StmtDeclNode*) = 0;

        virtual void visit(const VarDeclNode*) = 0;

        virtual void visit(const FunctionDeclNode*) = 0;

        virtual void visit(const ValueExprNode*) = 0;

        virtual void visit(const SymbolExprNode*) = 0;

        virtual void visit(const BinaryExprNode*) = 0;

        virtual void visit(const UnaryExprNode*) = 0;

        virtual void visit(const ProcCallExprNode*) = 0;

        virtual void visit(const AssignExprNode*) = 0;

        virtual void visit(const BlockStmtNode*) = 0;

        virtual void visit(const ExprStmtNode*) = 0;

        virtual void visit(const IfStmtNode*) = 0;

        virtual void visit(const WhileStmtNode*) = 0;

        virtual void visit(const BreakStmtNode*) = 0;

        virtual void visit(const ContinueStmtNode*) = 0;

        virtual void visit(const ReturnStmtNode*) = 0;
    };
}
