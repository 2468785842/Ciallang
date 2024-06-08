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
    std::unique_ptr<VM::VMChunk> CodeGen::parseAst(Common::Result& r,
                                                   const Syntax::AstNode* node) {
        _chunks.push_back(new VM::VMChunk{ _sourceFile.path() });
        _r = &r;

        node->accept(this);

        _chunks.back()->emit(VM::Opcode::Ret, node->location);

        if(_r->isFailed()) return nullptr;

        auto* p = _chunks.back();

        _chunks.pop_back();

        return std::make_unique<VM::VMChunk>(std::move(*p));
    }

    void CodeGen::visit(const Syntax::ExprStmtNode* node) {
        node->expression->accept(this);
    }

    void CodeGen::visit(const Syntax::ValueExprNode* node) {
        const auto val = node->token->value();

        // val <= 255, embed opcode
        if(val->type() == TjsValueType::Integer && val->asInteger() <= 255) {
            _chunks.back()->emit(VM::Opcode::Push,
                node->location,
                { static_cast<uint8_t>(val->asInteger()) }
            );
            return;
        }

        const auto index = _chunks.back()->addConstant(std::move(*node->token->value()));

        _chunks.back()->emit(VM::Opcode::Load, node->location, VM::encodeIEX(index));
    }

    void CodeGen::visit(const Syntax::BinaryExprNode* node) {
        const auto type = node->token->type();


        switch(type) {
            default: break;
            case Syntax::TokenType::LogicalAnd:
            case Syntax::TokenType::LogicalOr:
                node->lhs->accept(this);

                const auto op = type == Syntax::TokenType::LogicalAnd
                                ? VM::Opcode::JmpNE
                                : VM::Opcode::JmpE;
                const int16_t offset = _chunks.back()->emitJmp(op, node->location);

                _chunks.back()->emit(VM::Opcode::Pop, node->location);
                node->rhs->accept(this);
                _chunks.back()->patchJmp(offset);
                return;
        }

        node->lhs->accept(this);
        node->rhs->accept(this);

        switch(type) {
            default: break;
            case Syntax::TokenType::Plus:
                _chunks.back()->emit(VM::Opcode::Add, node->location);
                return;
            case Syntax::TokenType::Minus:
                _chunks.back()->emit(VM::Opcode::Sub, node->location);
                return;
            case Syntax::TokenType::Slash:
                _chunks.back()->emit(VM::Opcode::Div, node->location);
                return;
            case Syntax::TokenType::Asterisk:
                _chunks.back()->emit(VM::Opcode::Mul, node->location);
                return;
            case Syntax::TokenType::Equal:
                _chunks.back()->emit(VM::Opcode::Equals, node->location);
                return;
            case Syntax::TokenType::NotEqual:
                _chunks.back()->emit(VM::Opcode::NEquals, node->location);
                return;
        }

        LOG(FATAL) << "no impl `binary code gen`" << node->name();
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
            auto constIndex = _chunks.back()->addConstant(std::move(*identiter->value()));
            _chunks.back()->emit(VM::Opcode::DGlobal,
                node->location,
                VM::encodeIEX(constIndex)
            );
            return;
        }

        // assignment local var
        _chunks.back()->emit(VM::Opcode::DLocal, node->location, VM::encodeIEX(index));
        local->init = true;
    }

    void CodeGen::visit(const Syntax::VarDeclNode* node) {
        auto identitier = *node->token->value();
        auto index = _chunks.back()->addConstant(std::move(identitier));

        if(node->rhs)
            // has intialized assignement ?
            node->rhs->accept(this);
        else
            // no have initialized void
            _chunks.back()->emit(VM::Opcode::PVoid, node->location);

        // global scope
        if(_scopeDepth == 1) {
            _chunks.back()->emit(VM::Opcode::DGlobal, node->location, VM::encodeIEX(index));
            _chunks.back()->emit(VM::Opcode::Pop, node->location);
            return;
        }

        // local scope
        for(auto local : _locals | std::views::reverse) {
            // in different scope, can have same name variable
            // check this variable name. in this scope already have?
            if(local->depth < _scopeDepth) break;
            // in same scope. will check it!!
            if(*local->token == *node->token) {
                error(*_r,
                    "Already a variable with this name in the scope",
                    node->location);
                return;
            }
        }

        // XXX: fixed max length
        _locals.push_back(new Local{ node->token, _scopeDepth, !!node->rhs });
        _chunks.back()->emit(VM::Opcode::DLocal, node->location, VM::encodeIEX(_locals.size() - 1));
    }

    void CodeGen::visit(const Syntax::FunctionDeclNode* node) {
        auto name = node->token->value();

        auto chunk = parseAst(*_r, node->body);
        std::vector<VM::VMChunk> parameters{};

        for(const auto& paramNode : node->parameters) {
            std::unique_ptr<VM::VMChunk> paramChunk = nullptr;
            if(paramNode) {
                paramChunk = parseAst(*_r, paramNode);
                if(!paramChunk) return;
            }

            parameters.push_back(std::move(*paramChunk));
        }

        auto index = _chunks.back()->addConstant(TjsValue{
                TjsFunction{
                        std::move(parameters),
                        std::move(*chunk),
                        *name->asString()
                }
        });

        _chunks.back()->emit(VM::Opcode::Load, node->location, VM::encodeIEX(index));
        index = _chunks.back()->addConstant(std::move(*name));
        _chunks.back()->emit(VM::Opcode::DGlobal, node->location, VM::encodeIEX(index));
    }


    void CodeGen::visit(const Syntax::SymbolExprNode* node) {
        const auto& [index, local] = resolveLocal(node->token);

        // global variable maybe?
        if(!local) {
            auto constIndex = _chunks.back()->addConstant(std::move(*node->token->value()));
            _chunks.back()->emit(VM::Opcode::GGlobal, node->location, VM::encodeIEX(constIndex));
            return;
        }

        // not initialized
        if(!local->init) {
            error(*_r,
                "variable must be initialzed with use before(except in global scope)",
                node->location);

            return;
        }

        _chunks.back()->emit(VM::Opcode::GLocal, node->location, VM::encodeIEX(index));
    }

    void CodeGen::visit(const Syntax::StmtDeclNode* node) {
        node->statement->accept(this);
        // expr stmt will push value in stack top, but is unuseful
        if(dynamic_cast<const Syntax::ExprStmtNode*>(node->statement)) {
            _chunks.back()->emit(VM::Opcode::Pop, node->location);
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

        auto jmpAddr = _chunks.back()->emitJmp(VM::Opcode::JmpNE, line);
        node->body->accept(this);

        size_t addrOffset = 0;
        if(node->elseBody) {
            addrOffset = _chunks.back()->emitJmp(VM::Opcode::Jmp, line);
        }

        _chunks.back()->patchJmp(jmpAddr);

        if(node->elseBody) {
            node->elseBody->accept(this);
            _chunks.back()->patchJmp(addrOffset);
        }

        _chunks.back()->emit(VM::Opcode::Pop, line);
    }

    void CodeGen::visit(const Syntax::WhileStmtNode* node) {
        auto line = node->location;

        node->test->accept(this);

        auto jmpNeAddr = _chunks.back()->emitJmp(VM::Opcode::JmpNE, line);
        _chunks.back()->emit(VM::Opcode::Pop, line);

        _loops.push_back({ jmpNeAddr, _scopeDepth });
        node->body->accept(this);

        _chunks.back()->emitJmp(VM::Opcode::Jmp,
            line, static_cast<int16_t>(jmpNeAddr - _chunks.back()->count() - 6)
        );

        _chunks.back()->patchJmp(jmpNeAddr);
        _chunks.back()->emit(VM::Opcode::Pop, line);

        for(auto& addr : _loops.back().controls) {
            _chunks.back()->patchJmp(addr);
        }

        _loops.pop_back();
    }

    void CodeGen::visit(const Syntax::BreakStmtNode* node) {
        if(_loops.empty()) {
            error(*_r, "break; must be in loop Statement", node->location);
            return;
        }

        auto offset = _chunks.back()->emitJmp(VM::Opcode::Jmp, node->location);
        _loops.back().controls.push_back({ offset });
    }

    void CodeGen::visit(const Syntax::ContinueStmtNode* node) {
        if(_loops.empty()) {
            error(*_r, "continue; must be in loop Statement", node->location);
            return;
        }

        _chunks.back()->emitJmp(VM::Opcode::Jmp, node->location,
            static_cast<int16_t>(_loops.back().addr - _chunks.back()->count() - 6)
        );
    }

    void CodeGen::beginScope() {
        ++_scopeDepth;
    }

    void CodeGen::endScope(const Syntax::BlockStmtNode* node) {
        --_scopeDepth;

        if(_scopeDepth == 0) return;
        size_t count = 0;
        for(auto local : _locals | std::views::reverse) {
            if(local->depth <= _scopeDepth) break;

            ++count;
        }

        if(count == 0) return;

        size_t i = count;
        while(i-- > 0) _locals.pop_back();

        if(count > 255)
            DLOG(FATAL) << "to big > 255 ... popn scope.." << count;

        if(count == 1)
            _chunks.back()->emit(VM::Opcode::Pop, node->location);
        else
            _chunks.back()->emit(VM::Opcode::PopN, node->location, { static_cast<uint8_t>(count) });
    }

    std::pair<size_t, CodeGen::Local*> CodeGen::resolveLocal(const Syntax::Token* token) const {
        for(size_t i = 0; i < _locals.size(); i++) {
            auto* local = _locals[i];

            DCHECK(local->depth != 0);

            if(*local->token == *token) {
                return { i, local };
            }
        }
        return { 0, nullptr };
    }
}
