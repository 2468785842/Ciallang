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

#include "parser/ast/DeclNode.hpp"
#include "parser/ast/ExprNode.hpp"
#include "parser/ast/StmtNode.hpp"
#include "types/Class.hpp"
#include "types/Function.hpp"

#include "logging/Logger.hpp"
#include "vm/Instruction.hpp"

namespace cial::Inter {

    Opt<Bytecode::Chunk> IRGenerator::parseAst(const Common::Result &r, const Syntax::AstNode *node, OptReg &retReg) {
        _r = r;
        node->generateBytecode(this, retReg);
        if(_r.isFailed())
            return {};
        _chunk->setRegCount(_regNextIndex);
        auto chunk = std::move(_chunk);
        _chunk = std::make_unique<Bytecode::Chunk>();
        return Bytecode::Chunk{ std::move(*chunk.release()) };
    }

    void IRGenerator::generate(const Syntax::ExprStmtNode *node, OptReg &retReg) {
        return node->expression->generateBytecode(this, retReg);
    }

    void IRGenerator::generate(const Syntax::ValueExprNode *node, OptReg &retReg) {
        auto dst = allocateRegister();
        _chunk->emit<Bytecode::Op::OpCode::Load>(dst, _chunk->addConstant(node->token->constVal()));

        if(_r.isFailed())
            return;
        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::BinaryExprNode *node, OptReg &retReg) {
        using enum Syntax::TokenType;

        if(node->token->type() == Dot) {
            OptReg reg1{};
            node->lhs->generateBytecode(this, reg1);

            if(_r.isFailed())
                return;
            CLL_ASSERT(reg1, "reg1 is empty");
            Bytecode::Register dst = allocateRegister();

            if(auto *identifier = dynamic_cast<const Syntax::IdentifierExprNode *>(node->rhs); identifier) {
                _chunk->emit<Bytecode::Op::OpCode::Load>(dst, _chunk->addConstant(identifier->token->constVal()));
                _chunk->emit<Bytecode::Op::OpCode::GProp>(reg1.value(), dst, dst);
            } else {
                OptReg reg2{};
                node->rhs->generateBytecode(this, reg2);

                if(_r.isFailed())
                    return;
                CLL_ASSERT(reg2, "reg2 is empty");
                _chunk->emit<Bytecode::Op::OpCode::GProp>(reg1.value(), reg2.value(), dst);
            }
            retReg = dst;
            return;
        }

        OptReg reg1{};
        node->lhs->generateBytecode(this, reg1);
        OptReg reg2{};
        node->rhs->generateBytecode(this, reg2);

        auto dst = allocateRegister();

        if(_r.isFailed())
            return;

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
            case LogicalAnd:
                _chunk->emit<Bytecode::Op::OpCode::LAnd>(reg1.value(), reg2.value(), dst);
                break;
            case LogicalOr:
                _chunk->emit<Bytecode::Op::OpCode::LOr>(reg1.value(), reg2.value(), dst);
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
                return;
        }

        if(_r.isFailed())
            return;
        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::UnaryExprNode *node, OptReg &retReg) {
        using enum Syntax::TokenType;
        switch(node->token->type()) {
            case New:
                node->rhs->generateBytecode(this, retReg);
                break;
            case Exclamation:
                node->rhs->generateBytecode(this, retReg);
                _chunk->emit<Bytecode::Op::OpCode::LNot>(*retReg);
                break;
            case Minus:
                node->rhs->generateBytecode(this, retReg);
                _chunk->emit<Bytecode::Op::OpCode::ChS>(*retReg);
                break;
            default:
                CLL_LOG_ERROR("unknow unary operator");
        }
    }

    void IRGenerator::generate(const Syntax::ProcCallExprNode *node, OptReg &retReg) {
        auto *member = node->memberAccess;
        auto dst = allocateRegister();

        std::vector<Bytecode::Register> arguments{};
        OptReg memberReg{};
        member->generateBytecode(this, memberReg);

        CLL_ASSERT(memberReg, "memberReg is empty");
        for(const auto *exprNode : node->arguments) {
            if(!exprNode) {
                _chunk->emit<Bytecode::Op::OpCode::PushReg>(loadVoidReg(*_chunk));
            } else {
                OptReg reg;
                exprNode->generateBytecode(this, reg);
                if(_r.isFailed())
                    return;
                CLL_ASSERT(reg, "reg is empty");
                _chunk->emit<Bytecode::Op::OpCode::PushReg>(*reg);
            }
        }
        freeRegister(*memberReg);
        _chunk->emit<Bytecode::Op::OpCode::Call>(dst, *memberReg, node->arguments.size());
        if(!node->arguments.empty()) {
            _chunk->emit<Bytecode::Op::OpCode::PopN>(node->arguments.size());
        }
        retReg = dst;
    }


    void IRGenerator::generate(const Syntax::AssignExprNode *node, OptReg &retReg) {
        // TODO: member access
        const auto identifier = node->lhs->token->constVal();

        auto variable = resolveLocalVariable(identifier.value<Atom>());

        if(variable && variable.value()) {
            OptReg src{};
            node->rhs->generateBytecode(this, src);
            CLL_ASSERT(src.has_value(), "src is not have val");

            OptReg dst;
            node->lhs->generateBytecode(this, dst);
            CLL_ASSERT(dst.has_value(), "dst is not have val");

            freeRegister(src.value());
            _chunk->emit<Bytecode::Op::OpCode::Mov>(src.value(), dst.value());
            if(_r.isFailed())
                return;

            retReg = dst;
        }

        // global maybe
        OptReg src;
        node->rhs->generateBytecode(this, src);
        CLL_ASSERT(src.has_value(), "global src is not have val");

        _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier.value<Atom>(), src.value());

        if(_r.isFailed())
            return;
        retReg = src;
    }

    void IRGenerator::generate(const Syntax::VarDeclNode *node, OptReg &retReg) {
        const auto identifier = node->token->constVal().value<Atom>();

        // global
        if(isTopScope()) {
            // can't init
            if(!node->rhs) {
                _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, loadVoidReg(*_chunk));
                return;
            }

            OptReg src;
            node->rhs->generateBytecode(this, src);
            freeRegister(src.value());
            if(_r.isFailed())
                return;

            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, src.value());
            return;
        }

        auto variable = resolveLocalVariable(identifier);

        // already have this variable, in same scope
        if(variable.has_value() && variable.value()) {
            if(!node->rhs)
                return;

            OptReg src;
            node->rhs->generateBytecode(this, src);
            freeRegister(src.value());

            if(!_r.isFailed())
                return;

            CLL_ASSERT(src.has_value(), "src is not have val");

            _chunk->emit<Bytecode::Op::OpCode::Mov>(src.value(), variable.value()->reg);
            return;
        }

        OptReg dst = loadVoidReg(*_chunk);
        // can init
        if(node->rhs) {
            node->rhs->generateBytecode(this, dst);

            if(_r.isFailed())
                return;
            CLL_ASSERT(dst.has_value(), "src is not have val");
        }

        addLocalVar(LocalVariable{ identifier, dst.value(), getNextInstPos() });

        if(node->next)
            node->next->generateBytecode(this, retReg);
    }

    void IRGenerator::generate(const Syntax::FunctionDeclNode *node, OptReg &retReg) {
        auto funReg = allocateRegister();
        auto *funcMeta = generateChunk(node);

        const auto identifier = node->token->constVal().value<Atom>();

        _chunk->emit<Bytecode::Op::OpCode::Load>(funReg, _chunk->addConstant(Constant{ funcMeta }));

        if(isTopScope()) {
            freeRegister(funReg);
            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, funReg);
            return;
        }

        addLocalVar(LocalVariable{ identifier, funReg, getNextInstPos() });
    }


    void IRGenerator::generate(const Syntax::ClassDeclNode *node, OptReg &retReg) {
        // TODO:
        const auto identifier = node->token->constVal().value<Atom>();

        auto classReg = allocateRegister();
        auto *classMeta = _rt.createNoGC<ClassMeta>(identifier, 0);
        _chunk->emit<Bytecode::Op::OpCode::Load>(classReg, _chunk->addConstant(Constant{ classMeta }));

        // class is always global in current design
        _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, classReg);

        beginScope();
        for(const auto &declNode : node->body->childrens) {

            if(const auto *funcDeclNode = dynamic_cast<Syntax::FunctionDeclNode *>(declNode)) {
                const auto funName = funcDeclNode->token->constVal().value<Atom>();
                const auto funChunk = generateChunk(funcDeclNode);
                classMeta->setMember(MemberShapMeta{ funName }, funChunk);
            } else if(const auto *varDeclNode = dynamic_cast<Syntax::VarDeclNode *>(declNode)) {
                // TODO:
                // const auto varName = varDeclNode->token->constVal().value<Atom>();
                //
                // Bytecode::Register ret = allocateRegister();
                //
                // // can init
                // if(varDeclNode->rhs) {
                //     auto gen = IRGenerator{ _rt, _sourceFile };
                //     gen.makeVirtualGlobalScope();
                //     OptReg src;
                //     auto chunk = gen.parseAst(_r, varDeclNode->rhs, src);
                //     chunk->emit<Bytecode::Op::OpCode::Ret>(src.value());
                //     if(_r.isFailed())
                //         return;
                // }
                //
                // classMeta->setMember(MemberShapMeta{ varName }, _rt.createNoGC<PropMeta>(
                // _rt.createNoGC<FuncMeta>()
                // ));
            }
        }

        endScope();
        freeRegister(classReg);
    }

    void IRGenerator::generate(const Syntax::IdentifierExprNode *node, OptReg &retReg) {
        const auto identifier = node->token->constVal().value<Atom>();

        auto variable = resolveLocalVariable(identifier);

        if(variable.has_value()) {
            retReg = variable.value()->reg;
            return;
        }

        auto dst = allocateRegister();
        if(isTopScope()) {
            _chunk->emit<Bytecode::Op::OpCode::GGlobal>(identifier, dst);
        } else {
            // dynamic get
            // but tjs2 doesn't support get up function scope local var
            // _chunk->emit<Bytecode::Op::OpCode::GUpval>(identifier, dst);
            _chunk->emit<Bytecode::Op::OpCode::GThis>(identifier, dst);
        }

        if(_r.isFailed())
            return;

        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::StmtDeclNode *node, OptReg &retReg) {
        return node->statement->generateBytecode(this, retReg);
    }

    void IRGenerator::generate(const Syntax::BlockStmtNode *node, OptReg &retReg) {
        beginScope();
        for(const auto children : node->childrens) {
            children->generateBytecode(this, retReg);
            if(_r.isFailed())
                return;
        }
        endScope();
    }

    void IRGenerator::generate(const Syntax::IfStmtNode *node, OptReg &retReg) {
        OptReg testReg;
        node->test->generateBytecode(this, testReg);
        if(_r.isFailed())
            return;

        CLL_ASSERT(testReg.has_value(), "testReg is not have val");

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());

        auto *jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();
        node->body->generateBytecode(this, retReg);
        Bytecode::Op::Instruction *jmp = nullptr;

        if(node->elseBody) {
            jmp = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
        }

        Bytecode::Op::JmpNE::setTarget(*jmpNE, makeLabel());

        if(node->elseBody) {
            node->elseBody->generateBytecode(this, retReg);
            Bytecode::Op::Jmp::setTarget(*jmp, makeLabel());
        }

        freeRegister(testReg.value());
    }

    void IRGenerator::generate(const Syntax::SwitchStmtNode *node, OptReg &retReg) {}

    void IRGenerator::generate(const Syntax::DoWhileStmtNode *node, OptReg &retReg) {
        const auto bodyLabel = makeLabel();
        _loopStack.push_back(LoopContext{});

        node->body->generateBytecode(this, retReg);

        const auto testLabel = makeLabel();
        _loopStack.back().continueLabel = testLabel;
        for(auto *ct : _loopStack.back().continues) {
            Bytecode::Op::Jmp::setTarget(*ct, testLabel);
        }

        OptReg testReg;
        node->test->generateBytecode(this, testReg);
        if(_r.isFailed())
            return;
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
    }

    void IRGenerator::generate(const Syntax::ForStmtNode *node, OptReg &retReg) {
        if(node->init)
            node->init->generateBytecode(this, retReg);

        const auto testLabel = makeLabel();
        _loopStack.push_back(LoopContext{});

        Bytecode::Op::Instruction *jmpNE{ nullptr };
        OptReg testReg{};
        if(node->test) {
            node->test->generateBytecode(this, testReg);
            if(_r.isFailed())
                return;
            CLL_ASSERT(testReg.has_value(), "testReg is not have val");
            _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());
            jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();
        }

        node->body->generateBytecode(this, retReg);

        const auto stepLabel = makeLabel();
        _loopStack.back().continueLabel = stepLabel;
        for(auto *ct : _loopStack.back().continues) {
            Bytecode::Op::Jmp::setTarget(*ct, stepLabel);
        }

        if(node->step) {
            OptReg stepReg{};
            node->step->generateBytecode(this, stepReg);
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
    }

    void IRGenerator::generate(const Syntax::WhileStmtNode *node, OptReg &retReg) {

        const auto loopLabel = makeLabel();

        _loopStack.push_back(LoopContext{ .continueLabel = loopLabel });

        OptReg testReg;
        node->test->generateBytecode(this, testReg);
        if(_r.isFailed())
            return;

        CLL_ASSERT(testReg.has_value(), "testReg is not have val");

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg.value());
        auto *jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();

        node->body->generateBytecode(this, retReg);
        Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(), loopLabel);
        const auto exitLabel = makeLabel();
        Bytecode::Op::JmpNE::setTarget(*jmpNE, exitLabel);

        for(auto *br : _loopStack.back().breaks) {
            Bytecode::Op::Jmp::setTarget(*br, exitLabel);
        }

        freeRegister(testReg.value());

        _loopStack.pop_back();
    }

    void IRGenerator::generate([[maybe_unused]] const Syntax::BreakStmtNode *node, OptReg &retReg) {
        if(_loopStack.empty())
            return;
        auto *itt = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
        _loopStack.back().breaks.push_back(itt);
    }

    void IRGenerator::generate([[maybe_unused]] const Syntax::ContinueStmtNode *node, OptReg &retReg) {
        if(_loopStack.empty())
            return;
        if(_loopStack.back().continueLabel.has_value()) {
            Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(),
                                         _loopStack.back().continueLabel.value());
        } else {
            auto *itt = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
            _loopStack.back().continues.push_back(itt);
        }
    }

    void IRGenerator::generate(const Syntax::ReturnStmtNode *node, OptReg &retReg) {
        if(node->expr) {
            OptReg reg;
            node->expr->generateBytecode(this, reg);
            CLL_ASSERT(reg.has_value(), "reg is not have val");
            _chunk->emit<Bytecode::Op::OpCode::Ret>(reg.value());
            return;
        }
        _chunk->emit<Bytecode::Op::OpCode::Ret>(loadVoidReg(*_chunk));
    }

    std::optional<LocalVariable *> IRGenerator::resolveLocalVariable(const Atom identifier) {
        for(auto &var : std::ranges::reverse_view(_localVars)) {
            if(var.endPC == 0 && var.identifier.v == identifier.v) {
                return &var;
            }
        }
        return {};
    }

    FuncMeta *IRGenerator::generateChunk(const Syntax::FunctionDeclNode *node) const {

        auto gen = IRGenerator{ _rt, _sourceFile };
        gen.makeVirtualGlobalScope();

        OptReg paramReg{};

        for(auto &[token, exprNode] : node->parameters) {
            const auto varName = token.constVal().value<Atom>();

            if(exprNode) {
                OptReg defaultParameter{};
                exprNode->generateBytecode(&gen, defaultParameter);
                CLL_ASSERT(defaultParameter.has_value(), "defaultParameter is not have val");
                paramReg = defaultParameter.value();
            } else {
                paramReg = gen.allocateRegister();
            }
            const auto startPC = gen.getNextInstPos();
            gen.addLocalVar(LocalVariable{ varName, paramReg.value(), startPC });
        }

        OptReg ignoreReg{};
        auto funChunk = gen.parseAst(_r, node->body, ignoreReg);
        assert(funChunk);

        // the last instruction is not ret, patch one ret
        if(auto &instVec = funChunk->getInstVec();
           instVec.empty() || instVec.back()->opcode != Bytecode::Op::OpCode::Ret) {
            funChunk->emit<Bytecode::Op::OpCode::Ret>(gen.loadVoidReg(*funChunk));
        }

        auto *chunk = _rt.createNoGC<Bytecode::Chunk>(std::move(*funChunk));
        return _rt.createNoGC<FuncMeta>(static_cast<std::uint32_t>(node->parameters.size()), chunk,
                                        std::move(gen._localVars));
    }

    Bytecode::Register IRGenerator::loadVoidReg(Bytecode::Chunk &chunk) {
        if(!_empty.has_value()) {
            _empty = allocateRegister();
            chunk.emit<Bytecode::Op::OpCode::Load>(_empty.value(), ConstIdx{ 0 });
        }
        return _empty.value();
    }

} // namespace cial::Inter
