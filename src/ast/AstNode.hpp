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

#include <vector>

#include "../common/SourceLocation.hpp"

namespace Ciallang::Syntax {
    class AstNode;

    class ExprNode;

    class StmtNode;

    class ValueExprNode;

    class SymbolExprNode;

    class BinaryExprNode;

    class UnaryExprNode;

    class AssignExprNode;

    class BlockStmtNode;

    class ExprStmtNode;

    class IfStmtNode;

    using ExprNodeList = std::vector<ExprNode*>;
    using StmtNodeList = std::vector<StmtNode*>;

    class AstNode {
    public:
        struct Visitor;

        // just for Gen Graphviz used
        const uint64_t id = serialId++;

        SourceLocation location{};

        explicit AstNode() = default;

        virtual void accept(const Visitor* visitor) const = 0;

        [[nodiscard]] virtual constexpr const char* name() const noexcept = 0;

        virtual ~AstNode() = default;

    private:
        static inline uint64_t serialId = 0;
    };

    struct AstNode::Visitor {
        virtual ~Visitor() = default;

        virtual void visit(const ValueExprNode*) const = 0;

        virtual void visit(const SymbolExprNode*) const = 0;

        virtual void visit(const BinaryExprNode*) const = 0;

        virtual void visit(const UnaryExprNode*) const = 0;

        virtual void visit(const AssignExprNode*) const = 0;

        virtual void visit(const BlockStmtNode*) const = 0;

        virtual void visit(const ExprStmtNode*) const = 0;

        virtual void visit(const IfStmtNode*) const = 0;
    };
}
