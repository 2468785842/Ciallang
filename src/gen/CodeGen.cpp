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
        _vmChunk->emit(VM::Opcode::Ret, node->location);
        if(_r->isFailed()) _vmChunk->reset();
    }

    void CodeGen::visit(const Syntax::ExprStmtNode* node) {
        node->expression->accept(this);
    }

    void CodeGen::visit(const Syntax::ValueExprNode* node) {
        auto val = node->token->value();

        // val <= 255, embed opcode
        if(val->type() == TjsValueType::Integer && val->asInteger() <= 255) {
            _vmChunk->emit(VM::Opcode::Push,
                node->location,
                { static_cast<uint8_t>(val->asInteger()) }
            );
            return;
        }

        auto index = _vmChunk->addConstant(std::move(*node->token->value()));

        _vmChunk->emit(VM::Opcode::Load, node->location, VM::encodeIEX(index));
    }

    void CodeGen::visit(const Syntax::BinaryExprNode* node) {
        auto type = node->token->type();

        if(type == Syntax::TokenType::LogicalAnd) {
            node->lhs->accept(this);
            auto offset = _vmChunk->emitJmp(VM::Opcode::JmpNE, node->location);
            _vmChunk->emit(VM::Opcode::Pop, node->location);
            node->rhs->accept(this);
            _vmChunk->patchJmp(offset);
            return;
        }

        if(type == Syntax::TokenType::LogicalOr) {
            node->lhs->accept(this);
            auto offset = _vmChunk->emitJmp(VM::Opcode::JmpE, node->location);
            _vmChunk->emit(VM::Opcode::Pop, node->location);
            node->rhs->accept(this);
            _vmChunk->patchJmp(offset);
            return;
        }

        node->lhs->accept(this);
        node->rhs->accept(this);

        switch(type) {
            case Syntax::TokenType::Plus:
                _vmChunk->emit(VM::Opcode::Add, node->location);
                return;
            case Syntax::TokenType::Minus:
                _vmChunk->emit(VM::Opcode::Sub, node->location);
                return;
            case Syntax::TokenType::Slash:
                _vmChunk->emit(VM::Opcode::Div, node->location);
                return;
            case Syntax::TokenType::Asterisk:
                _vmChunk->emit(VM::Opcode::Mul, node->location);
            default: ;
        }
    }

    void CodeGen::visit(const Syntax::UnaryExprNode*) {
        LOG(FATAL) << "no impl";
    }

    void CodeGen::visit(const Syntax::AssignExprNode* node) {
        auto* identiter = node->lhs->token;

        node->rhs->accept(this);

        const auto& [index, local] = resolveLocal(identiter);

        if(!local) {
            // global scope maybe? check it in runtime
            auto constIndex = _vmChunk->addConstant(std::move(*identiter->value()));
            _vmChunk->emit(VM::Opcode::DGlobal, node->location, VM::encodeIEX(constIndex));
            return;
        }

        // assignment local var
        _vmChunk->emit(VM::Opcode::DLocal, node->location, VM::encodeIEX(index));
        local->init = true;
    }

    void CodeGen::visit(const Syntax::VarDeclNode* node) {
        auto identitier = *node->token->value();
        auto index = _vmChunk->addConstant(std::move(identitier));

        if(node->rhs)
            // has intialized assignement ?
            node->rhs->accept(this);
        else
            // no have initialized void
            _vmChunk->emit(VM::Opcode::PVoid, node->location);

        // global scope
        if(scopeDepth == 1) {
            _vmChunk->emit(VM::Opcode::DGlobal, node->location, VM::encodeIEX(index));
            _vmChunk->emit(VM::Opcode::Pop, node->location);
            return;
        }

        // local scope
        for(auto local : locals | std::views::reverse) {
            // in different scope, can have same name variable
            // check this variable name. in this scope already have?
            if(local->depth < scopeDepth) break;
            // in same scope. will check it!!
            if(*local->token == *node->token) {
                error(*_r,
                    "Already a variable with this name in the scope",
                    node->location);
                return;
            }
        }

        // XXX: fixed max length
        locals.push_back(new Local{ node->token, scopeDepth, !!node->rhs });
        _vmChunk->emit(VM::Opcode::DLocal, node->location, VM::encodeIEX(locals.size() - 1));
    }

    void CodeGen::visit(const Syntax::SymbolExprNode* node) {
        const auto& [index, local] = resolveLocal(node->token);

        // global variable maybe?
        if(!local) {
            auto constIndex = _vmChunk->addConstant(std::move(*node->token->value()));
            _vmChunk->emit(VM::Opcode::GGlobal, node->location, VM::encodeIEX(constIndex));
            return;
        }

        // not initialized
        if(!local->init) {
            error(*_r,
                "variable must be initialzed with use before(except in global scope)",
                node->location);

            return;
        }

        _vmChunk->emit(VM::Opcode::GLocal, node->location, VM::encodeIEX(index));
    }

    void CodeGen::visit(const Syntax::StmtDeclNode* node) {
        node->statement->accept(this);
        // expr stmt will push value in stack top, but is unuseful
        if(dynamic_cast<const Syntax::ExprStmtNode*>(node->statement)) {
            _vmChunk->emit(VM::Opcode::Pop, node->location);
        }
    }

    void CodeGen::visit(const Syntax::BlockStmtNode* node) {
        beginScope();
        for(const auto* children : node->childrens) {
            children->accept(this);
        }
        endScope(node);
    }

    void CodeGen::visit(const Syntax::IfStmtNode* node) {
        auto line = node->location;
        node->test->accept(this);

        auto jmpAddr = _vmChunk->emitJmp(VM::Opcode::JmpNE, line);
        node->body->accept(this);

        size_t addrOffset = 0;
        if(node->elseBody) {
            addrOffset = _vmChunk->emitJmp(VM::Opcode::Jmp, line);
        }

        _vmChunk->patchJmp(jmpAddr);

        if(node->elseBody) {
            node->elseBody->accept(this);
            _vmChunk->patchJmp(addrOffset);
        }

        _vmChunk->emit(VM::Opcode::Pop, line);
    }

    void CodeGen::visit(const Syntax::WhileStmtNode* node) {
        auto line = node->location;

        node->test->accept(this);

        auto jmpNeAddr = _vmChunk->emitJmp(VM::Opcode::JmpNE, line);
        _vmChunk->emit(VM::Opcode::Pop, line);

        loops.push_back({ jmpNeAddr, scopeDepth });
        node->body->accept(this);

        _vmChunk->emitJmp(VM::Opcode::Jmp,
            line, static_cast<int16_t>(jmpNeAddr - _vmChunk->count() - 6)
        );

        _vmChunk->patchJmp(jmpNeAddr);
        _vmChunk->emit(VM::Opcode::Pop, line);

        for(auto& addr : loops.back().controls) {
            _vmChunk->patchJmp(addr);
        }

        loops.pop_back();
    }

    void CodeGen::visit(const Syntax::BreakStmtNode* node) {
        if(loops.empty()) {
            error(*_r, "break; must be in loop Statement", node->location);
            return;
        }

        auto offset = _vmChunk->emitJmp(VM::Opcode::Jmp, node->location);
        loops.back().controls.push_back({ offset });
    }

    void CodeGen::visit(const Syntax::ContinueStmtNode* node) {
        if(loops.empty()) {
            error(*_r, "continue; must be in loop Statement", node->location);
            return;
        }

        _vmChunk->emitJmp(VM::Opcode::Jmp, node->location,
            static_cast<int16_t>(loops.back().addr - _vmChunk->count() - 6)
        );
    }

    void CodeGen::beginScope() {
        ++scopeDepth;
    }

    void CodeGen::endScope(const Syntax::BlockStmtNode* node) {
        --scopeDepth;

        if(scopeDepth == 0) return;
        size_t count = 0;
        for(auto local : locals | std::views::reverse) {
            if(local->depth <= scopeDepth) break;

            ++count;
        }

        if(count == 0) return;

        size_t i = count;
        while(i-- > 0) locals.pop_back();

        if(count > 255)
            DLOG(FATAL) << "to big > 255 ... popn scope.." << count;

        if(count == 1)
            _vmChunk->emit(VM::Opcode::Pop, node->location);
        else
            _vmChunk->emit(VM::Opcode::PopN, node->location, { static_cast<uint8_t>(count) });
    }

    std::pair<size_t, CodeGen::Local*> CodeGen::resolveLocal(const Syntax::Token* token) const {
        for(size_t i = 0; i < locals.size(); i++) {
            auto* local = locals[i];

            DCHECK(local->depth != 0);

            if(*local->token == *token) {
                return { i, local };
            }
        }
        return { 0, nullptr };
    }
}
