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

#include "IRGenerator.hpp"

#include "ast/DeclNode.hpp"
#include "ast/ExprNode.hpp"
#include "ast/StmtNode.hpp"
#include "types/Function.hpp"

#include "logging/Logger.hpp"
#include "vm/Instruction.hpp"

namespace Ciallang::Inter {

    std::unique_ptr<Bytecode::Chunk> IRGenerator::parseAst(const Common::Result &r, const Syntax::AstNode *node) {
        _r = r;
        node->generateBytecode(this);
        if(_r.isFailed()) {
            return nullptr;
        }
        auto chunk = std::move(_chunk);
        _chunk = std::make_unique<Bytecode::Chunk>();
        return chunk;
    }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::ExprStmtNode *node) {
        return node->expression->generateBytecode(this);
    }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::ValueExprNode *node) {
        auto dst = allocateRegister();
        _chunk->emit<Bytecode::Op::OpCode::Load>(dst, node->token->value());

        if(_r.isFailed())
            return {};
        return dst;
    }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::BinaryExprNode *node) {
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

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::UnaryExprNode *node) { return {}; }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::ProcCallExprNode *node) {
        auto *member = node->memberAccess;
        auto dst = allocateRegister();

        if(dynamic_cast<const Syntax::IdentifierExprNode *>(member)) {
            std::vector<Bytecode::Register> arguments{};
            auto memberReg = node->memberAccess->generateBytecode(this);

            CLL_ASSERT(memberReg, "memberReg is empty");
            for(size_t i = node->arguments.size(); i > 0; i--) {
                const auto *exprNode = node->arguments[i - 1];
                if(!exprNode) {
                    _chunk->emit<Bytecode::Op::OpCode::PushReg>(getEmpty(*_chunk));
                } else {
                    auto reg = exprNode->generateBytecode(this);
                    if(_r.isFailed())
                        return {};
                    CLL_ASSERT(reg, "reg is empty");
                    _chunk->emit<Bytecode::Op::OpCode::PushReg>(*reg);
                }
            }
            freeRegister(*memberReg);
            _chunk->emit<Bytecode::Op::OpCode::Call>(dst, *memberReg, node->arguments.size());
            return dst;
        }
        CLL_LOG_ERROR("not impl");
        return {};
    }


    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::AssignExprNode *node) {
        // TODO: member access
        const auto identifier = node->lhs->token->value();

        CLL_ASSERT(identifier.isString(), "identifier is not string");

        auto variable = resolveLocalVariable(*identifier.toString());

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

        _chunk->emit<Bytecode::Op::OpCode::DGlobal>(_symbolTable.getOrAddSymbol(*identifier.toString()), src.value());

        if(_r.isFailed())
            return {};
        return src;
    }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::VarDeclNode *node) {
        const auto identifier = node->token->value();

        CLL_ASSERT(identifier.isString(), "identifier is not string");

        // global
        if(_scopeDepth == 1) {
            // can't init
            if(!node->rhs) {
                _chunk->emit<Bytecode::Op::OpCode::DGlobal>(_symbolTable.getOrAddSymbol(*identifier.toString()),
                                                            getEmpty(*_chunk));
                return {};
            }

            auto src = node->rhs->generateBytecode(this);
            freeRegister(src.value());
            if(_r.isFailed())
                return {};

            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(_symbolTable.getOrAddSymbol(*identifier.toString()),
                                                        src.value());
            return {};
        }

        auto variable = resolveLocalVariable(*identifier.toString());

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

        _variables.emplace_back(std::move(*identifier.toString()), dst.value(), _scopeDepth, !!node->rhs);

        return {};
    }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::FunctionDeclNode *node) {
        auto gen = IRGenerator{ _sourceFile, _symbolTable };

        for(auto &[token, exprNode] : node->parameters) {
            const auto varName = token.value();
            std::optional<Bytecode::Register> paramReg{};

            CLL_ASSERT(varName.isString(), "varName is not string");

            if(exprNode) {
                auto defaultParameter = exprNode->generateBytecode(&gen);
                CLL_ASSERT(defaultParameter.has_value(), "defaultParameter is not have val");
                paramReg = defaultParameter.value();
            } else {
                paramReg = gen.allocateRegister();
            }

            gen.addVariable(LocalVariable{ *varName.toString(), paramReg.value(), 1, true });
        }

        auto funReg = allocateRegister();
        auto funChunk = gen.parseAst(_r, node->body);

        // the last instruction is not ret, patch one ret
        if(funChunk->instructions().back().opcode != Bytecode::Op::OpCode::Ret) {
            funChunk->emit<Bytecode::Op::OpCode::Ret>(gen.getEmpty(*funChunk));
        }

        const auto identifier = node->token->value();

        CLL_ASSERT(identifier.isString(), "identifier is not string");

        _chunk->emit<Bytecode::Op::OpCode::Load>(
            funReg, TjsValue{ new Function{ funChunk.release(), *identifier.toString(), node->parameters.size() } });

        if(_scopeDepth == 1) {
            freeRegister(funReg);
            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(_symbolTable.getOrAddSymbol(*identifier.toString()), funReg);
            return {};
        }

        _variables.push_back(LocalVariable{ *identifier.toString(), funReg, _scopeDepth, true });

        return {};
    }


    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::IdentifierExprNode *node) {
        const auto identifier = node->token->value();

        CLL_ASSERT(identifier.isString(), "identifier is not string");

        auto variable = resolveLocalVariable(*identifier.toString());

        if(variable.has_value()) {
            if(!variable.value()->init) {
                error(_r, "variable no initialization", node->location);
                return {};
            }
            return variable.value()->reg;
        }

        auto dst = allocateRegister();
        _chunk->emit<Bytecode::Op::OpCode::GGlobal>(_symbolTable.getOrAddSymbol(*identifier.toString()), dst);
        if(_r.isFailed())
            return {};

        return dst;
    }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::StmtDeclNode *node) {
        return node->statement->generateBytecode(this);
    }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::BlockStmtNode *node) {
        beginScope();
        for(const auto children : node->childrens) {
            children->generateBytecode(this);
            if(_r.isFailed())
                return {};
        }
        endScope();

        return {};
    }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::IfStmtNode *node) {
        auto testReg = node->test->generateBytecode(this);
        if(_r.isFailed())
            return {};

        CLL_ASSERT(testReg.has_value(), "testReg is not have val");

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());

        const size_t jmpNEIndex = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();
        node->body->generateBytecode(this);
        size_t jmpIndex = 0;

        if(node->elseBody) {
            jmpIndex = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
        }

        Bytecode::Op::JmpNE::setTarget(_chunk->getItt(jmpNEIndex), makeLabel());

        if(node->elseBody) {
            node->elseBody->generateBytecode(this);
            Bytecode::Op::Jmp::setTarget(_chunk->getItt(jmpIndex), makeLabel());
        }

        freeRegister(testReg.value());

        return {};
    }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::WhileStmtNode *node) {
        const auto loopLabel = makeLabel();
        auto testReg = node->test->generateBytecode(this);
        if(_r.isFailed())
            return {};

        CLL_ASSERT(testReg.has_value(), "testReg is not have val");

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());
        size_t jmpNEIndex = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();

        node->body->generateBytecode(this);
        Bytecode::Op::Jmp::setTarget(_chunk->getItt(_chunk->emit<Bytecode::Op::OpCode::Jmp>()), loopLabel);

        Bytecode::Op::Jmp::setTarget(_chunk->getItt(_chunk->emit<Bytecode::Op::OpCode::Jmp>()), makeLabel());
        Bytecode::Op::JmpNE::setTarget(_chunk->getItt(jmpNEIndex), makeLabel());

        freeRegister(testReg.value());

        return {};
    }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::BreakStmtNode *node) { return {}; }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::ContinueStmtNode *node) { return {}; }

    std::optional<Bytecode::Register> IRGenerator::generate(const Syntax::ReturnStmtNode *node) {
        if(node->expr) {
            auto reg = node->expr->generateBytecode(this);
            CLL_ASSERT(reg.has_value(), "reg is not have val");
            _chunk->emit<Bytecode::Op::OpCode::Ret>(reg.value());
            return {};
        }
        _chunk->emit<Bytecode::Op::OpCode::Ret>(getEmpty(*_chunk));
        return {};
    }

    std::optional<LocalVariable *> IRGenerator::resolveLocalVariable(const TjsString &identifier) {
        for(auto &variable : _variables) {
            if(variable.identifier == identifier && variable.scopeDepth <= _scopeDepth) {
                return &variable;
            }
        }
        return {};
    }
} // namespace Ciallang::Inter
