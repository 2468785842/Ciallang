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

#include <ranges>

#include "ast/DeclNode.hpp"
#include "ast/ExprNode.hpp"
#include "ast/StmtNode.hpp"
#include "types/Class.hpp"
#include "types/Function.hpp"

#include "logging/Logger.hpp"
#include "vm/Instruction.hpp"

namespace Cial::Inter {

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

    Syntax::OptReg IRGenerator::generate(const Syntax::ExprStmtNode *node) {
        return node->expression->generateBytecode(this);
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::ValueExprNode *node) {
        auto dst = allocateRegister();
        _chunk->emit<Bytecode::Op::OpCode::Load>(dst, _chunk->addConstant(node->token->constVal()));

        if(_r.isFailed())
            return {};
        return dst;
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::BinaryExprNode *node) {
        using enum Syntax::TokenType;

        if(node->token->type() == Dot) {
            auto reg1 = node->lhs->generateBytecode(this);

            if(_r.isFailed())
                return {};
            CLL_ASSERT(reg1, "reg1 is empty");
            Bytecode::Register dst = allocateRegister();

            if(auto *identifier = dynamic_cast<const Syntax::IdentifierExprNode *>(node->rhs); identifier) {
                _chunk->emit<Bytecode::Op::OpCode::GProp>(reg1.value(),
                                                          _chunk->addConstant(identifier->token->constVal()), dst);
            } else {
                auto reg2 = node->rhs->generateBytecode(this);

                if(_r.isFailed())
                    return {};
                CLL_ASSERT(reg2, "reg2 is empty");
                _chunk->emit<Bytecode::Op::OpCode::GProp>(reg1.value(), reg2.value(), dst);
            }
            return dst;
        }

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

    Syntax::OptReg IRGenerator::generate(const Syntax::UnaryExprNode *node) {
        using enum Syntax::TokenType;
        switch(node->token->type()) {
            case New:
                return node->rhs->generateBytecode(this);
            default:
                CLL_LOG_ERROR("unknow unary operator");
                return {};
        }

        return {};
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::ProcCallExprNode *node) {
        auto *member = node->memberAccess;
        auto dst = allocateRegister();

        std::vector<Bytecode::Register> arguments{};
        auto memberReg = member->generateBytecode(this);

        CLL_ASSERT(memberReg, "memberReg is empty");
        for(const auto *exprNode : node->arguments) {
            if(!exprNode) {
                _chunk->emit<Bytecode::Op::OpCode::PushReg>(loadVoidReg(*_chunk));
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
        if(!node->arguments.empty()) {
            _chunk->emit<Bytecode::Op::OpCode::PopN>(node->arguments.size());
        }
        return dst;
    }


    Syntax::OptReg IRGenerator::generate(const Syntax::AssignExprNode *node) {
        // TODO: member access
        const auto identifier = node->lhs->token->constVal();

        auto variable = resolveLocalVariable(identifier.value<Atom>());

        if(variable && variable.value()) {
            auto src = node->rhs->generateBytecode(this);
            CLL_ASSERT(src.has_value(), "src is not have val");

            auto dst = node->lhs->generateBytecode(this);
            CLL_ASSERT(dst.has_value(), "dst is not have val");

            freeRegister(src.value());
            _chunk->emit<Bytecode::Op::OpCode::Mov>(src.value(), dst.value());
            if(_r.isFailed())
                return {};

            return dst;
        }

        // global maybe
        auto src = node->rhs->generateBytecode(this);
        CLL_ASSERT(src.has_value(), "global src is not have val");

        _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier.value<Atom>(), src.value());

        if(_r.isFailed())
            return {};
        return src;
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::VarDeclNode *node) {
        const auto identifier = node->token->constVal().value<Atom>();

        // global
        if(isTopScope()) {
            // can't init
            if(!node->rhs) {
                _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, loadVoidReg(*_chunk));
                return {};
            }

            auto src = node->rhs->generateBytecode(this);
            freeRegister(src.value());
            if(_r.isFailed())
                return {};

            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, src.value());
            return {};
        }

        auto variable = resolveLocalVariable(identifier);

        // already have this variable, in same scope
        if(variable.has_value() && variable.value()) {
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

        Syntax::OptReg dst{};
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
            dst = loadVoidReg(*_chunk);
        }

        addLocalVariable(FuncMeta::LocalVariable{ identifier, dst.value(), getNextInstPos() });

        return {};
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::FunctionDeclNode *node) {
        auto funReg = allocateRegister();
        auto funChunk = generateChunk(node);

        const auto identifier = node->token->constVal().value<Atom>();

        _chunk->emit<Bytecode::Op::OpCode::Load>(funReg,
                                                 _chunk->addConstant(Constant{ new FuncMeta(std::move(funChunk)) }));

        if(isTopScope()) {
            freeRegister(funReg);
            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, funReg);
            return {};
        }

        addLocalVariable(FuncMeta::LocalVariable{ identifier, funReg, getNextInstPos() });

        return {};
    }


    Syntax::OptReg IRGenerator::generate(const Syntax::ClassDeclNode *node) {
        // TODO:
        // const auto identifier = node->token->value().value<Atom>();
        //
        // auto thisObjReg = allocateRegister();
        // const Value classObjVal = Object::create<ClassObject>(identifier);
        // _chunk->emit<Bytecode::Op::OpCode::Load>(thisObjReg, _chunk->addConstant(classObjVal));
        //
        // // class is always global in current design
        // _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier,
        //                                             thisObjReg);
        // auto *classObject = dynamic_cast<ClassObject *>(classObjVal.toObject());
        //
        // beginScope();
        // for(const auto &declNode : node->body->childrens) {
        //     addVariable(Variable{ identifier });
        //
        //     if(const auto *funcDeclNode = dynamic_cast<Syntax::FunctionDeclNode *>(declNode)) {
        //         auto funChunk = generateChunk(funcDeclNode);
        //         auto funName = funcDeclNode->token->value().value<Atom>();
        //         classObject->setMethod(
        //             funName, Object::create<Function>(funChunk.release(), funName, funcDeclNode->parameters.size()));
        //     } else if(const auto *varDeclNode = dynamic_cast<Syntax::VarDeclNode *>(declNode)) {
        //         const auto varName = varDeclNode->token->value();
        //
        //         CLL_ASSERT(varName.isString(), "identifier is not string");
        //
        //         Syntax::OptReg src;
        //
        //         // can init
        //         if(varDeclNode->rhs) {
        //             src = varDeclNode->rhs->generateBytecode(this);
        //             // freeRegister(src.value());
        //             if(_r.isFailed())
        //                 return {};
        //         }
        //
        //         classObject->setFieldDef(varName.toString()->toStdStr(), FieldMeta{ src });
        //     }
        // }
        //
        // endScope();
        // freeRegister(thisObjReg);
        return {};
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::IdentifierExprNode *node) {
        const auto identifier = node->token->constVal().value<Atom>();

        auto variable = resolveLocalVariable(identifier);

        if(variable.has_value()) {
            return variable.value()->reg;
        }

        // dynamic get
        auto dst = allocateRegister();
        _chunk->emit<Bytecode::Op::OpCode::GDynamic>(identifier, dst);
        if(_r.isFailed())
            return {};

        return dst;
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::StmtDeclNode *node) {
        return node->statement->generateBytecode(this);
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::BlockStmtNode *node) {
        beginScope();
        for(const auto children : node->childrens) {
            children->generateBytecode(this);
            if(_r.isFailed())
                return {};
        }
        endScope();

        return {};
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::IfStmtNode *node) {
        auto testReg = node->test->generateBytecode(this);
        if(_r.isFailed())
            return {};

        CLL_ASSERT(testReg.has_value(), "testReg is not have val");

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());

        auto *jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();
        node->body->generateBytecode(this);
        Bytecode::Op::Instruction *jmp = nullptr;

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

    Syntax::OptReg IRGenerator::generate(const Syntax::DoWhileStmtNode *node) {
        const auto bodyLabel = makeLabel();
        _loopStack.push_back(LoopContext{});

        node->body->generateBytecode(this);

        const auto testLabel = makeLabel();
        _loopStack.back().continueLabel = testLabel;
        for(auto *ct : _loopStack.back().continues) {
            Bytecode::Op::Jmp::setTarget(*ct, testLabel);
        }

        auto testReg = node->test->generateBytecode(this);
        if(_r.isFailed())
            return {};
        CLL_ASSERT(testReg.has_value(), "testReg is not have val");

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());
        auto *jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();
        Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(), bodyLabel);

        const auto exitLabel = makeLabel();
        Bytecode::Op::JmpNE::setTarget(*jmpNE, exitLabel);
        for(auto *br : _loopStack.back().breaks) {
            Bytecode::Op::Jmp::setTarget(*br, exitLabel);
        }

        freeRegister(testReg.value());
        _loopStack.pop_back();
        return {};
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::ForStmtNode *node) {
        if(node->init)
            node->init->generateBytecode(this);

        const auto testLabel = makeLabel();
        _loopStack.push_back(LoopContext{});

        Bytecode::Op::Instruction *jmpNE{ nullptr };
        Syntax::OptReg testReg{};
        if(node->test) {
            testReg = node->test->generateBytecode(this);
            if(_r.isFailed())
                return {};
            CLL_ASSERT(testReg.has_value(), "testReg is not have val");
            _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());
            jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();
        }

        node->body->generateBytecode(this);

        const auto stepLabel = makeLabel();
        _loopStack.back().continueLabel = stepLabel;
        for(auto *ct : _loopStack.back().continues) {
            Bytecode::Op::Jmp::setTarget(*ct, stepLabel);
        }

        if(node->step) {
            auto stepReg = node->step->generateBytecode(this);
            if(stepReg)
                freeRegister(stepReg.value());
        }
        Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(), testLabel);

        const auto exitLabel = makeLabel();
        if(jmpNE)
            Bytecode::Op::JmpNE::setTarget(*jmpNE, exitLabel);
        for(auto *br : _loopStack.back().breaks) {
            Bytecode::Op::Jmp::setTarget(*br, exitLabel);
        }

        if(testReg)
            freeRegister(testReg.value());
        _loopStack.pop_back();
        return {};
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::WhileStmtNode *node) {

        const auto loopLabel = makeLabel();

        _loopStack.push_back(LoopContext{ .continueLabel = loopLabel });

        auto testReg = node->test->generateBytecode(this);
        if(_r.isFailed())
            return {};

        CLL_ASSERT(testReg.has_value(), "testReg is not have val");

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());
        auto *jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();

        node->body->generateBytecode(this);
        Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(), loopLabel);
        const auto exitLabel = makeLabel();
        Bytecode::Op::JmpNE::setTarget(*jmpNE, exitLabel);

        for(auto *br : _loopStack.back().breaks) {
            Bytecode::Op::Jmp::setTarget(*br, exitLabel);
        }

        freeRegister(testReg.value());

        _loopStack.pop_back();
        return {};
    }

    Syntax::OptReg IRGenerator::generate([[maybe_unused]] const Syntax::BreakStmtNode *node) {
        if(_loopStack.empty())
            return {};
        auto *itt = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
        _loopStack.back().breaks.push_back(itt);
        return {};
    }

    Syntax::OptReg IRGenerator::generate([[maybe_unused]] const Syntax::ContinueStmtNode *node) {
        if(_loopStack.empty())
            return {};
        if(_loopStack.back().continueLabel.has_value()) {
            Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(),
                                         _loopStack.back().continueLabel.value());
        } else {
            auto *itt = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
            _loopStack.back().continues.push_back(itt);
        }
        return {};
    }

    Syntax::OptReg IRGenerator::generate(const Syntax::ReturnStmtNode *node) {
        if(node->expr) {
            auto reg = node->expr->generateBytecode(this);
            CLL_ASSERT(reg.has_value(), "reg is not have val");
            _chunk->emit<Bytecode::Op::OpCode::Ret>(reg.value());
            return {};
        }
        _chunk->emit<Bytecode::Op::OpCode::Ret>(loadVoidReg(*_chunk));
        return {};
    }

    std::optional<FuncMeta::LocalVariable *> IRGenerator::resolveLocalVariable(const Atom identifier) {
        for(auto &var : std::ranges::reverse_view(_localVars)) {
            if(var.endPC == 0 && var.identifier.v == identifier.v) {
                return &var;
            }
        }
        return {};
    }

    FuncMeta IRGenerator::generateChunk(const Syntax::FunctionDeclNode *node) const {

        auto gen = IRGenerator{ _sourceFile };

        gen.beginScope();

        Syntax::OptReg paramReg{};

        for(auto &[token, exprNode] : node->parameters) {
            const auto varName = token.constVal().value<Atom>();

            if(exprNode) {
                auto defaultParameter = exprNode->generateBytecode(&gen);
                CLL_ASSERT(defaultParameter.has_value(), "defaultParameter is not have val");
                paramReg = defaultParameter.value();
            } else {
                paramReg = gen.allocateRegister();
            }
            const auto startPC = gen.getNextInstPos();
            gen.addLocalVariable(FuncMeta::LocalVariable{ varName, paramReg.value(), startPC });
        }

        auto funChunk = gen.parseAst(_r, node->body);
        gen.endScope();

        // the last instruction is not ret, patch one ret
        if(auto &instVec = funChunk->getInstVec();
           instVec.empty() || instVec.back()->opcode != Bytecode::Op::OpCode::Ret) {
            funChunk->emit<Bytecode::Op::OpCode::Ret>(gen.loadVoidReg(*funChunk));
        }

        return FuncMeta{ node->token->constVal().value<Atom>(), static_cast<std::uint32_t>(node->parameters.size()),
                         funChunk.release(), std::move(gen._localVars) };
    }

    Bytecode::Register IRGenerator::loadVoidReg(Bytecode::Chunk &chunk) {
        if(!_empty.has_value()) {
            _empty = allocateRegister();
            chunk.emit<Bytecode::Op::OpCode::Load>(_empty.value(), ConstIdx{ 0 });
        }
        return _empty.value();
    }

} // namespace Cial::Inter
