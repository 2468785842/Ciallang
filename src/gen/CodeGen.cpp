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

#include "CodeGen.hpp"

#include "../vm/Instruction.hpp"

namespace Ciallang::Inter {
    void CodeGen::loadAst(const Syntax::AstNode* node) const {
        node->accept(this);
        _vmChunk->emit(node->location.start().line, VM::Opcodes::Ret);
    }

    void CodeGen::visit(const Syntax::BlockStmtNode* node) const {
        for(const auto* children : node->childrens) {
            children->accept(this);
        }
    }

    void CodeGen::visit(const Syntax::ExprStmtNode* node) const {
        for(auto expression : node->expressions) {
            expression->accept(this);
        }
    }

    void CodeGen::visit(const Syntax::ValueExprNode* node) const {
        auto line = node->location.start().line;
        auto index = _vmChunk->load(std::move(*node->token->value()));
        _vmChunk->emit(line, VM::Opcodes::Const, { static_cast<uint8_t>(index) });
    }

    void CodeGen::visit(const Syntax::BinaryExprNode* node) const {
        auto line = node->location.start().line;

        node->lhs->accept(this);
        node->rhs->accept(this);

        switch(node->token->type()) {
            case Syntax::TokenType::Plus:
                _vmChunk->emit(line, VM::Opcodes::Add);
                break;
            case Syntax::TokenType::Minus:
                _vmChunk->emit(line, VM::Opcodes::Sub);
                break;
            case Syntax::TokenType::Slash:
                _vmChunk->emit(line, VM::Opcodes::Div);
                break;
            case Syntax::TokenType::Asterisk:
                _vmChunk->emit(line, VM::Opcodes::Mul);
                break;
            default: ;
        }
    }

    void CodeGen::visit(const Syntax::UnaryExprNode*) const {
        LOG(FATAL) << "no impl";
    }

    void CodeGen::visit(const Syntax::AssignExprNode*) const {
        LOG(FATAL) << "no impl";
    }

    void CodeGen::visit(const Syntax::VarDeclNode*) const {
        LOG(FATAL) << "no impl";
    }

    void CodeGen::visit(const Syntax::IfStmtNode*) const {
        LOG(FATAL) << "no impl";
    }

    void CodeGen::visit(const Syntax::SymbolExprNode*) const {
        LOG(FATAL) << "no impl";
    }
}
