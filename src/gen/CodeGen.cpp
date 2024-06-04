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
    void CodeGen::loadAst(Common::Result& r, const Syntax::AstNode* node) {
        _r = &r;
        node->accept(this);
        _vmChunk->emit(VM::Opcodes::Ret, node->location);
        if(_r->isFailed()) _vmChunk->reset();
        _r = nullptr;
    }

    void CodeGen::visit(const Syntax::BlockStmtNode* node) {
        beginScope();
        for(const auto* children : node->childrens) {
            children->accept(this);
        }
        endScope(node);
    }

    void CodeGen::visit(const Syntax::ExprStmtNode* node) {
        for(auto expression : node->expressions) {
            expression->accept(this);
        }
    }

    void CodeGen::visit(const Syntax::ValueExprNode* node) {
        auto val = node->token->value();

        // val <= 255, embed opcode
        if(val->type() == TjsValueType::Integer && val->asInteger() <= 255) {
            _vmChunk->emit(VM::Opcodes::Push,
                { static_cast<uint8_t>(val->asInteger()) },
                node->location
            );
            return;
        }

        auto index = _vmChunk->addConstant(std::move(*node->token->value()));

        _vmChunk->emit(VM::Opcodes::Const, { index }, node->location);
    }

    void CodeGen::visit(const Syntax::BinaryExprNode* node) {

        node->lhs->accept(this);
        node->rhs->accept(this);

        switch(node->token->type()) {
            case Syntax::TokenType::Plus:
                _vmChunk->emit(VM::Opcodes::Add, node->location);
                break;
            case Syntax::TokenType::Minus:
                _vmChunk->emit( VM::Opcodes::Sub, node->location);
                break;
            case Syntax::TokenType::Slash:
                _vmChunk->emit(VM::Opcodes::Div, node->location);
                break;
            case Syntax::TokenType::Asterisk:
                _vmChunk->emit(VM::Opcodes::Mul, node->location);
                break;
            default: ;
        }
    }

    void CodeGen::visit(const Syntax::UnaryExprNode*) {
        LOG(FATAL) << "no impl";
    }

    void CodeGen::visit(const Syntax::AssignExprNode* node) {
        auto* identiter = node->lhs->token;

        node->rhs->accept(this);

        auto depth = resolveLocal(identiter);

        // -1 not found in local scope, maybe in global scope
        if(depth == -1) {
            auto index = _vmChunk->addConstant(std::move(*identiter->value()));
            _vmChunk->emit(VM::Opcodes::DGlobal, { index }, node->location);
            return;
        }

        // initialized or assignment ?
        locals[locals.size() - 1].depth = scopeDepth;
        _vmChunk->emit( VM::Opcodes::DLocal, { depth }, node->location);
    }

    void CodeGen::visit(const Syntax::VarDeclNode* node) {
        auto* identitier = node->token->value();
        auto index = _vmChunk->addConstant(std::move(*identitier));

        if(node->rhs)
            // has assignement ?
            node->rhs->accept(this);
        else
            // no have initialized void
            _vmChunk->emit(VM::Opcodes::PVoid, node->location);

        // global scope
        if(scopeDepth == 1) {
            _vmChunk->emit(VM::Opcodes::DGlobal, { index }, node->location);
            return;
        }

        // local scope
        for(auto local : locals) {
            // in different scope, can have same name variable
            // check this variable name. in this scope already have?
            if(local.depth != -1 && local.depth < scopeDepth) {
                break;
            }
            // in same scope. will check it!!
            if(*local.token == *node->token) {
                error(*_r,
                    "Already a variable with this name in the scope",
                    node->location);
                return;
            }
        }

        // XXX: fixed max length
        locals.push_back(Local{ node->token, node->rhs ? static_cast<uint8_t>(scopeDepth) : -1 });
        _vmChunk->emit(VM::Opcodes::DLocal, { locals.size() - 1 }, node->location);
    }

    void CodeGen::visit(const Syntax::IfStmtNode*) {
        LOG(FATAL) << "no impl";
    }

    void CodeGen::visit(const Syntax::SymbolExprNode*) {
        LOG(FATAL) << "no impl";
    }

    void CodeGen::beginScope() {
        ++scopeDepth;
    }

    void CodeGen::endScope(const Syntax::BlockStmtNode* node) {
        --scopeDepth;
        size_t count = std::ranges::count_if(
            locals.begin(), locals.end(),
            [&](auto local) {
                return local.depth > scopeDepth;
            });
        _vmChunk->emit(VM::Opcodes::PopN, { count }, node->location);
    }

    TjsInteger CodeGen::resolveLocal(const Syntax::Token* token) const {
        for(TjsInteger i = 0; i < locals.size(); i++) {
            if(*locals[i].token == *token) {
                if(locals[i].depth == -1) {
                    error(*_r,
                        "Can't read local variable in its own initializer.",
                        token->location);
                }
                return i;
            }
        }
        return -1;
    }
}
