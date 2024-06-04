// Copyright (c) 2024/5/21 下午8:33
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

#include "../ast/Ast.hpp"
#include "../vm/Interpreter.hpp"

namespace Ciallang::Inter {

    class CodeGen;

    class CodeGen :  Syntax::AstNode::Visitor {
    public:
        friend class Syntax::AstNode;
        explicit CodeGen(VM::VMChunk* vmChunk) : _vmChunk(vmChunk) {
        }

        void loadAst(const Syntax::AstNode* node) const;

    private:
        VM::VMChunk* _vmChunk;

        void visit(const Syntax::ValueExprNode*) const override;

        void visit(const Syntax::SymbolExprNode*) const override;

        void visit(const Syntax::BinaryExprNode*) const override;

        void visit(const Syntax::UnaryExprNode*) const override;

        void visit(const Syntax::AssignExprNode*) const override;

        void visit(const Syntax::BlockStmtNode*) const override;

        void visit(const Syntax::ExprStmtNode*) const override;

        void visit(const Syntax::IfStmtNode*) const override;

        void visit(const Syntax::VarDeclNode*) const override;
    };
}
