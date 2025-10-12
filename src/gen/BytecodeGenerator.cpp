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

#include "BytecodeGenerator.hpp"

#include "ast/DeclNode.hpp"
#include "ast/ExprNode.hpp"
#include "ast/StmtNode.hpp"
#include "types/TjsFunction.hpp"

#include "logging/Logger.hpp"
#include "vm/Instruction.hpp"

namespace Ciallang::Inter {

    std::unique_ptr<Bytecode::Chunk> BytecodeGenerator::parseAst(const Common::Result &r, const Syntax::AstNode *node) {
        _r = r;
        node->generateBytecode(this);
        if(_r.isFailed()) {
            return nullptr;
        }
        auto chunk = std::move(_chunk);
        _chunk = std::make_unique<Bytecode::Chunk>();
        return chunk;
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::ExprStmtNode *node) {
        return node->expression->generateBytecode(this);
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::ValueExprNode *node) {
        auto dst = allocateRegister();
        _chunk->emit<Bytecode::Op::OpCode::Load>(dst, std::move(*node->token->value()));

        if(_r.isFailed())
            return {};
        return dst;
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::BinaryExprNode *node) {
        using enum Syntax::TokenType;
        auto reg1 = node->lhs->generateBytecode(this);
        auto reg2 = node->rhs->generateBytecode(this);

        auto dst = allocateRegister();

        if(_r.isFailed())
            return {};

        CLL_ASSERT(reg1.has_value(), "reg1 is empty");
        CLL_ASSERT(reg2.has_value(), "reg2 is empty");

        switch(node->token->type()) {
            case Equal:
                _chunk->emit<Bytecode::Op::OpCode::EQ>(reg1.value(), reg2.value(), dst);
                break;
            case NotEqual:
                _chunk->emit<Bytecode::Op::OpCode::NEQ>(reg1.value(), reg2.value(), dst);
                break;
            case Gt:
                _chunk->emit<Bytecode::Op::OpCode::GT>(reg1.value(), reg2.value(), dst);
                break;
            case GtOrEqual:
                _chunk->emit<Bytecode::Op::OpCode::GE>(reg1.value(), reg2.value(), dst);
                break;
            case Lt:
                _chunk->emit<Bytecode::Op::OpCode::LT>(reg1.value(), reg2.value(), dst);
                break;
            case LtOrEqual:
                _chunk->emit<Bytecode::Op::OpCode::LE>(reg1.value(), reg2.value(), dst);
                break;
            case Plus:
                _chunk->emit<Bytecode::Op::OpCode::Add>(reg1.value(), reg2.value(), dst);
                break;
            case Minus:
                _chunk->emit<Bytecode::Op::OpCode::Sub>(reg1.value(), reg2.value(), dst);
                break;
            case Asterisk:
                _chunk->emit<Bytecode::Op::OpCode::Mul>(reg1.value(), reg2.value(), dst);
                break;
            case Slash:
                _chunk->emit<Bytecode::Op::OpCode::Div>(reg1.value(), reg2.value(), dst);
                break;
            default:
                CLL_LOG_ERROR("unknow binary operator");
                return {};
        }

        if(_r.isFailed())
            return {};
        return dst;
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::UnaryExprNode *node) { return {}; }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::ProcCallExprNode *node) {
        auto *member = node->memberAccess;
        auto dst = allocateRegister();

        if(dynamic_cast<const Syntax::IdentifierExprNode *>(member)) {
            auto arguments = std::make_unique<std::vector<Bytecode::Register>>();
            auto memberReg = node->memberAccess->generateBytecode(this);

            CLL_ASSERT(memberReg.has_value(), "memberReg is empty");

            for(const auto *exprNode : node->arguments) {
                if(!exprNode) {
                    arguments->push_back(getEmpty(*_chunk));
                } else {
                    auto reg = exprNode->generateBytecode(this);
                    if(_r.isFailed())
                        return {};

                    CLL_ASSERT(reg.has_value(), "reg is empty");
                    freeRegister(reg.value());
                    arguments->push_back(reg.value());
                }
            }

            freeRegister(memberReg.value());
            _chunk->emit<Bytecode::Op::OpCode::Call>(dst, memberReg.value(), arguments.release());
            return dst;
        }
        CLL_LOG_ERROR("not impl");
        return {};
    }


    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::AssignExprNode *node) {
        // TODO: member access
        const auto identifier = node->lhs->token->value();

        CLL_ASSERT(identifier->isString(), "identifier is not string");

        auto variable = resolveLocalVariable(*identifier->toString());

        if(variable.has_value()) {
            auto src = node->rhs->generateBytecode(this);
            CLL_ASSERT(src.has_value(), "src is not have val");

            auto dst = node->lhs->generateBytecode(this);
            CLL_ASSERT(dst.has_value(), "dst is not have val");

            freeRegister(src.value());
            _chunk->emit<Bytecode::Op::OpCode::Mov>(src.value(), dst.value());
            variable.value()->init = true;
            if(_r.isFailed())
                return {};

            return dst;
        }

        // global maybe
        auto src = node->rhs->generateBytecode(this);
        CLL_ASSERT(src.has_value(), "global src is not have val");

        _chunk->emit<Bytecode::Op::OpCode::DGlobal>(_symbolTable.getOrAddSymbol(*identifier->toString()), src.value());

        if(_r.isFailed())
            return {};
        return src;
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::VarDeclNode *node) {
        const auto identifier = node->token->value();

        CLL_ASSERT(identifier->isString(), "identifier is not string");

        // global
        if(_scopeDepth == 1) {
            // can't init
            if(!node->rhs) {
                _chunk->emit<Bytecode::Op::OpCode::DGlobal>(_symbolTable.getOrAddSymbol(*identifier->toString()),
                                                            getEmpty(*_chunk));
                return {};
            }

            auto src = node->rhs->generateBytecode(this);
            freeRegister(src.value());
            if(_r.isFailed())
                return {};

            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(_symbolTable.getOrAddSymbol(*identifier->toString()),
                                                        src.value());
            return {};
        }

        auto variable = resolveLocalVariable(*identifier->toString());

        // already have this variable, in same scope
        if(variable.has_value()) {
            if(!node->rhs)
                return {};

            auto src = node->rhs->generateBytecode(this);
            freeRegister(src.value());

            if(!_r.isFailed())
                return {};

            CLL_ASSERT(src.has_value(), "src is not have val");

            _chunk->emit<Bytecode::Op::OpCode::Mov>(src.value(), variable.value()->reg);
            return {};
        }

        std::optional<Bytecode::Register> dst{};
        // can init
        if(node->rhs) {
            dst = allocateRegister();
            auto src = node->rhs->generateBytecode(this);
            freeRegister(src.value());

            if(_r.isFailed())
                return {};
            CLL_ASSERT(src.has_value(), "src is not have val");

            _chunk->emit<Bytecode::Op::OpCode::Mov>(src.value(), dst.value());
        } else {
            dst = getEmpty(*_chunk);
        }

        _variables.emplace_back(std::move(*identifier->toString()), dst.value(), _scopeDepth, !!node->rhs);

        return {};
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::FunctionDeclNode *node) {
        auto gen = BytecodeGenerator{ _sourceFile, _symbolTable };

        for(auto &[token, exprNode] : node->parameters) {
            const auto varName = token.value();
            std::optional<Bytecode::Register> paramReg{};

            CLL_ASSERT(varName->isString(), "varName is not string");

            if(exprNode) {
                auto defaultParameter = exprNode->generateBytecode(&gen);
                CLL_ASSERT(defaultParameter.has_value(), "defaultParameter is not have val");
                paramReg = defaultParameter.value();
            } else {
                paramReg = gen.allocateRegister();
            }

            gen.addVariable(LocalVariable{ *varName->toString(), paramReg.value(), 1, true });
        }

        auto funReg = allocateRegister();
        auto funChunk = gen.parseAst(_r, node->body);

        // the last instruction is not ret, patch one ret
        if(funChunk->instructions().back()->opcode != Bytecode::Op::OpCode::Ret) {
            funChunk->emit<Bytecode::Op::OpCode::Ret>(gen.getEmpty(*funChunk));
        }

        const auto identifier = node->token->value();

        CLL_ASSERT(identifier->isString(), "identifier is not string");

        _chunk->emit<Bytecode::Op::OpCode::Load>(
            funReg,
            TjsValue{ new TjsFunction{ funChunk.release(), *identifier->toString(), node->parameters.size() } });

        if(_scopeDepth == 1) {
            freeRegister(funReg);
            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(_symbolTable.getOrAddSymbol(*identifier->toString()), funReg);
            return {};
        }

        _variables.push_back(LocalVariable{ *identifier->toString(), funReg, _scopeDepth, true });

        return {};
    }


    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::IdentifierExprNode *node) {
        const auto identifier = node->token->value();

        CLL_ASSERT(identifier->isString(), "identifier is not string");

        auto variable = resolveLocalVariable(*identifier->toString());

        if(variable.has_value()) {
            if(!variable.value()->init) {
                error(_r, "variable no initialization", node->location);
                return {};
            }
            return variable.value()->reg;
        }

        auto dst = allocateRegister();
        _chunk->emit<Bytecode::Op::OpCode::GGlobal>(_symbolTable.getOrAddSymbol(*identifier->toString()), dst);
        if(_r.isFailed())
            return {};

        return dst;
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::StmtDeclNode *node) {
        return node->statement->generateBytecode(this);
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::BlockStmtNode *node) {
        beginScope();
        for(const auto children : node->childrens) {
            children->generateBytecode(this);
            if(_r.isFailed())
                return {};
        }
        endScope();

        return {};
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::IfStmtNode *node) {
        auto testReg = node->test->generateBytecode(this);
        if(_r.isFailed())
            return {};

        CLL_ASSERT(testReg.has_value(), "testReg is not have val");

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());

        auto *jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();
        node->body->generateBytecode(this);
        Bytecode::Op::Instruction *jmp{ nullptr };

        if(node->elseBody) {
            jmp = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
        }

        Bytecode::Op::JmpNE::setTarget(*jmpNE, makeLabel());

        if(node->elseBody) {
            node->elseBody->generateBytecode(this);
            Bytecode::Op::Jmp::setTarget(*jmp, makeLabel());
        }

        freeRegister(testReg.value());

        return {};
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::WhileStmtNode *node) {
        const auto loopLabel = makeLabel();
        auto testReg = node->test->generateBytecode(this);
        if(_r.isFailed())
            return {};

        CLL_ASSERT(testReg.has_value(), "testReg is not have val");

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());
        const auto jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();

        node->body->generateBytecode(this);
        Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(), loopLabel);

        Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(), makeLabel());
        Bytecode::Op::JmpNE::setTarget(*jmpNE, makeLabel());

        freeRegister(testReg.value());

        return {};
    }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::BreakStmtNode *node) { return {}; }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::ContinueStmtNode *node) { return {}; }

    std::optional<Bytecode::Register> BytecodeGenerator::generate(const Syntax::ReturnStmtNode *node) {
        if(node->expr) {
            auto reg = node->expr->generateBytecode(this);
            CLL_ASSERT(reg.has_value(), "reg is not have val");
            _chunk->emit<Bytecode::Op::OpCode::Ret>(reg.value());
            return {};
        }
        _chunk->emit<Bytecode::Op::OpCode::Ret>(getEmpty(*_chunk));
        return {};
    }

    std::optional<LocalVariable *> BytecodeGenerator::resolveLocalVariable(const TjsString &identifier) {
        for(auto &variable : _variables) {
            if(variable.identifier == identifier && variable.scopeDepth <= _scopeDepth) {
                return &variable;
            }
        }
        return {};
    }
} // namespace Ciallang::Inter
