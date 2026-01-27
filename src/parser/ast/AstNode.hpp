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

#include "../../gen/Register.hpp"
#include "common/SourceLocation.hpp"
#include "parser/Token.hpp"

#define CIAL_AST_NODE_ENUMS(O)                                                                                         \
    O(DeclNode)                                                                                                        \
    O(ExprNode)                                                                                                        \
    O(StmtNode)                                                                                                        \
    O(ValueExprNode)                                                                                                   \
    O(IdentifierExprNode)                                                                                              \
    O(InternalIdentifierExprNode)                                                                                      \
    O(BinaryExprNode)                                                                                                  \
    O(PrefixUnaryExprNode)                                                                                             \
    O(SuffixUnaryExprNode)                                                                                             \
    O(ProcCallExprNode)                                                                                                \
    O(AssignExprNode)                                                                                                  \
    O(FunctionExprNode)                                                                                                \
    O(BlockStmtNode)                                                                                                   \
    O(ExprStmtNode)                                                                                                    \
    O(TryStmtNode)                                                                                                     \
    O(IfStmtNode)                                                                                                      \
    O(SwitchStmtNode)                                                                                                  \
    O(DoWhileStmtNode)                                                                                                 \
    O(ForStmtNode)                                                                                                     \
    O(WhileStmtNode)                                                                                                   \
    O(BreakStmtNode)                                                                                                   \
    O(ContinueStmtNode)                                                                                                \
    O(ReturnStmtNode)                                                                                                  \
    O(DebuggerStmtNode)                                                                                                \
    O(TernaryExprNode)                                                                                                 \
    O(PropertyDeclNode)                                                                                                \
    O(VarDeclNode)                                                                                                     \
    O(FunctionDeclNode)                                                                                                \
    O(ClassDeclNode)                                                                                                   \
    O(StmtDeclNode)

namespace cial::inter {
    class IRGenerator;
}

namespace cial::syntax {
    class AstNode;

#define DECLARE_AST_NODE_CLASS(name) class name;
    CIAL_AST_NODE_ENUMS(DECLARE_AST_NODE_CLASS)
#undef DECLARE_AST_NODE_CLASS

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

        virtual void generateBytecode(inter::IRGenerator *, OptReg &) const = 0;

        [[nodiscard]] std::string_view name() const noexcept { return _name; }

        virtual ~AstNode() = default;

    private:
        const std::string _name{};
    };

    struct AstNode::Visitor {
        virtual ~Visitor() = default;

#define DECLARE_AST_NODE_VISIT(name) virtual void visit(const name *) = 0;
        CIAL_AST_NODE_ENUMS(DECLARE_AST_NODE_VISIT)
#undef DECLARE_AST_NODE_VISIT
    };
} // namespace cial::syntax
