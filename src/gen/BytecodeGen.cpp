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

#include "BytecodeGen.hpp"

#include "ast/DeclNode.hpp"
#include "ast/StmtNode.hpp"
#include "ast/ExprNode.hpp"
#include "types/TjsFunction.hpp"

#include "vm/Instruction.hpp"

namespace Ciallang::Inter {
    std::unique_ptr<Bytecode::Chunk> BytecodeGen::parseAst(
        const Common::Result& r,
        const Syntax::AstNode* node
    ) {
        _r = r;
        _chunk = new Bytecode::Chunk{};
        node->generateBytecode(this);
        if(_r.isFailed()) return nullptr;
        auto chunk = std::make_unique<Bytecode::Chunk>(std::move(*_chunk));
        _chunk = nullptr;
        return chunk;
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::ExprStmtNode* node) {
        return node->expression->generateBytecode(this);
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::ValueExprNode* node) {
        auto dst = allocateRegister();
        _chunk->emit<Bytecode::Op::Load>(dst, std::move(*node->token->value()));

        if(_r.isFailed()) return {};
        return dst;
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::BinaryExprNode* node) {
        using enum Syntax::TokenType;
        auto reg1 = node->lhs->generateBytecode(this);
        auto reg2 = node->rhs->generateBytecode(this);
        auto dst = allocateRegister();

        if(_r.isFailed()) return {};

        CHECK(reg1.has_value());
        CHECK(reg2.has_value());

        switch(node->token->type()) {
            case Equal:
                _chunk->emit<Bytecode::Op::EQ>(reg1.value(), reg2.value(), dst);
                break;
            case NotEqual:
                _chunk->emit<Bytecode::Op::NEQ>(reg1.value(), reg2.value(), dst);
                break;
            case Gt:
                _chunk->emit<Bytecode::Op::GT>(reg1.value(), reg2.value(), dst);
                break;
            case GtOrEqual:
                _chunk->emit<Bytecode::Op::GE>(reg1.value(), reg2.value(), dst);
                break;
            case Lt:
                _chunk->emit<Bytecode::Op::LT>(reg1.value(), reg2.value(), dst);
                break;
            case LtOrEqual:
                _chunk->emit<Bytecode::Op::LE>(reg1.value(), reg2.value(), dst);
                break;
            case Plus:
                _chunk->emit<Bytecode::Op::Add>(reg1.value(), reg2.value(), dst);
                break;
            case Minus:
                _chunk->emit<Bytecode::Op::Sub>(reg1.value(), reg2.value(), dst);
                break;
            case Asterisk:
                _chunk->emit<Bytecode::Op::Mul>(reg1.value(), reg2.value(), dst);
                break;
            case Slash:
                _chunk->emit<Bytecode::Op::Div>(reg1.value(), reg2.value(), dst);
                break;
            default:
                DCHECK(false) << "unknow binary operator";
                return {};
        }

        if(_r.isFailed()) return {};
        return dst;
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::UnaryExprNode* node) {
        return {};
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::ProcCallExprNode* node) {
        auto* member = node->memberAccess;
        auto dst = allocateRegister();

        if(dynamic_cast<const Syntax::IdentifierExprNode*>(member)) {
            std::vector<Bytecode::Register> arguments{};
            auto memberReg = node->memberAccess->generateBytecode(this);
            for(auto* exprNode : node->arguments) {
                if(!exprNode) {
                    arguments.push_back(getEmpty(*_chunk));
                } else {
                    auto reg = exprNode->generateBytecode(this);
                    if(_r.isFailed()) return {};

                    CHECK(reg.has_value());
                    arguments.push_back(reg.value());
                }
            }
            _chunk->emit<Bytecode::Op::Call>(dst, memberReg.value(), std::move(arguments));
            return dst;
        }
        DCHECK(true) << "no impl";
        return {};
    }


    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::AssignExprNode* node) {
        // TODO: member access
        auto identifier = node->lhs->token->value();

        DCHECK(identifier->isString());

        auto variable = resolveLocalVariable(*identifier->asString());

        if(variable.has_value()) {
            auto dst = node->lhs->generateBytecode(this);
            auto src = node->rhs->generateBytecode(this);
            DCHECK(dst.has_value());
            DCHECK(src.has_value());
            _chunk->emit<Bytecode::Op::Mov>(src.value(), dst.value());
            variable.value()->init = true;
            if(_r.isFailed()) return {};

            return dst;
        }

        // global maybe
        auto src = node->rhs->generateBytecode(this);
        DCHECK(src.has_value());

        _chunk->emit<Bytecode::Op::DGlobal>(std::move(*identifier->asString()), src.value());

        if(_r.isFailed()) return {};
        return src;
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::VarDeclNode* node) {
        auto identifier = node->token->value();

        DCHECK(identifier->isString());

        // global
        if(_scopeDepth == 1) {
            // can't init
            if(!node->rhs) {
                _chunk->emit<Bytecode::Op::DGlobal>(
                    std::move(*identifier->asString()), getEmpty(*_chunk)
                );
                return {};
            }

            auto src = node->rhs->generateBytecode(this);
            if(_r.isFailed()) return {};

            _chunk->emit<Bytecode::Op::DGlobal>(
                std::move(*identifier->asString()), src.value()
            );
            return {};
        }

        auto variable = resolveLocalVariable(*identifier->asString());

        // already have this variable, in same scope
        if(variable.has_value()) {
            if(!node->rhs) return {};

            auto src = node->rhs->generateBytecode(this);

            if(!_r.isFailed()) return {};

            DCHECK(src.has_value());

            _chunk->emit<Bytecode::Op::Mov>(src.value(), variable.value()->reg);
            return {};
        }

        std::optional<Bytecode::Register> dst{};
        // can init
        if(node->rhs) {
            dst = allocateRegister();
            auto src = node->rhs->generateBytecode(this);

            if(_r.isFailed()) return {};
            CHECK(src.has_value());

            _chunk->emit<Bytecode::Op::Mov>(src.value(), dst.value());
        } else {
            dst = getEmpty(*_chunk);
        }

        _variables.emplace_back(
            std::move(*identifier->asString()),
            dst.value(), _scopeDepth, !!node->rhs
        );

        return {};
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::FunctionDeclNode* node) {
        auto gen = BytecodeGen{ _sourceFile };

        for(auto& [token, exprNode] : node->parameters) {
            auto varName = token.value();
            std::optional<Bytecode::Register> paramReg{};

            CHECK(varName->isString());

            if(exprNode) {
                auto defaultParameter = exprNode->generateBytecode(&gen);
                CHECK(defaultParameter.has_value());
                paramReg = defaultParameter.value();
            } else {
                paramReg = gen.allocateRegister();
            }

            gen.addVariable(LocalVariable{
                    *varName->asString(),
                    paramReg.value(),
                    1,
                    true
            });
        }

        auto funReg = allocateRegister();
        auto funChunk = gen.parseAst(_r, node->body);

        // the last instruction is not ret, patch one ret
        if(!dynamic_cast<Bytecode::Op::Ret*>(funChunk->instructions().back())) {
            funChunk->emit<Bytecode::Op::Ret>(gen.getEmpty(*funChunk));
        }

        auto identifier = node->token->value();

        CHECK(identifier->isString());

        if(_scopeDepth == 1) {
            _chunk->emit<Bytecode::Op::Load>(funReg, TjsValue{ TjsFunction{
                            std::move(*funChunk.release()),
                            *identifier->asString(),
                            node->parameters.size()
                    }
            });

            _chunk->emit<Bytecode::Op::DGlobal>(
                std::move(*identifier->asString()),
                funReg
            );
            freeRegister(funReg);
            return {};
        }

        _chunk->emit<Bytecode::Op::Load>(funReg, TjsValue{ TjsFunction{
                        std::move(*funChunk.release()),
                        *identifier->asString(),
                        node->parameters.size()
                }
        });

        _variables.push_back(LocalVariable{
                *identifier->asString(),
                funReg,
                _scopeDepth,
                true
        });

        return {};
    }


    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::IdentifierExprNode* node) {
        auto identifier = node->token->value();

        DCHECK(identifier->isString());

        auto variable = resolveLocalVariable(*identifier->asString());

        if(variable.has_value()) {
            if(!variable.value()->init) {
                error(_r, "variable no initialization", node->location);
                return {};
            }
            return variable.value()->reg;
        }

        auto dst = allocateRegister();
        _chunk->emit<Bytecode::Op::GGlobal>(std::move(*identifier->asString()), dst);
        if(_r.isFailed()) return {};

        return dst;
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::StmtDeclNode* node) {
        return node->statement->generateBytecode(this);
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::BlockStmtNode* node) {
        beginScope();
        for(auto children : node->childrens) {
            children->generateBytecode(this);
            if(_r.isFailed()) return {};
        }
        endScope();

        return {};
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::IfStmtNode* node) {
        auto testReg = node->test->generateBytecode(this);
        if(_r.isFailed()) return {};

        DCHECK(testReg.has_value());

        _chunk->emit<Bytecode::Op::Test>(testReg.value());

        auto* jmpNE = _chunk->emit<Bytecode::Op::JmpNE>();
        node->body->generateBytecode(this);
        Bytecode::Op::Jmp* jmp{ nullptr };

        if(node->elseBody) {
            jmp = _chunk->emit<Bytecode::Op::Jmp>();
        }

        jmpNE->setTarget(makeLabel());

        if(node->elseBody) {
            node->elseBody->generateBytecode(this);
            jmp->setTarget(makeLabel());
        }

        return {};
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::WhileStmtNode* node) {
        auto loopLabel = makeLabel();
        auto testReg = node->test->generateBytecode(this);
        if(_r.isFailed()) return {};

        DCHECK(testReg.has_value());

        _chunk->emit<Bytecode::Op::Test>(testReg.value());
        auto jmpNE = _chunk->emit<Bytecode::Op::JmpNE>();

        node->body->generateBytecode(this);
        _chunk->emit<Bytecode::Op::Jmp>()->setTarget(loopLabel);

        jmpNE->setTarget(makeLabel());

        freeRegister(testReg.value());

        return {};
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::BreakStmtNode* node) {
        return {};
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::ContinueStmtNode* node) {
        return {};
    }

    std::optional<Bytecode::Register> BytecodeGen::generate(const Syntax::ReturnStmtNode* node) {
        if(node->expr) {
            auto reg = node->expr->generateBytecode(this);
            _chunk->emit<Bytecode::Op::Ret>(reg.value());
            return {};
        }
        _chunk->emit<Bytecode::Op::Ret>(getEmpty(*_chunk));
        return {};
    }

    std::optional<LocalVariable*> BytecodeGen::resolveLocalVariable(const std::string& identifier) {
        for(auto& variable : _variables) {
            if(variable.identifier == identifier
               && variable.scopeDepth <= _scopeDepth) {
                return &variable;
            }
        }
        return {};
    }
}
