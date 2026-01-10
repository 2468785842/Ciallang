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

#include "common/Defer.hpp"
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

    bool IRGenerator::expectValue(const Syntax::ExprNode *node, Bytecode::Register &ret) {
        OptReg reg{};
        node->generateBytecode(this, reg);

        if(!reg) {
            error("except a return value of expression", node->location);
            return false;
        }

        ret = *reg;
        return true;
    }

    void IRGenerator::generate(const Syntax::ExprStmtNode *node, OptReg &retReg) {
        return node->expression->generateBytecode(this, retReg);
    }

    void IRGenerator::generate(const Syntax::ValueExprNode *node, OptReg &retReg) {
        if(node->token.type() == Syntax::TokenType::Null) {
            error("null token current not support", node->location);
            return;
        }
        auto dst = allocateRegister();

        _chunk->emit<Bytecode::Op::OpCode::Load>(dst, _chunk->addConstant(constVal(node->token)));
        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::BinaryExprNode *node, OptReg &retReg) {
        using enum Syntax::TokenType;

        if(node->token.type() == Dot) {
            Bytecode::Register reg1{ 0 };
            if(!expectValue(node->lhs, reg1))
                return;

            Bytecode::Register dst = allocateRegister();

            if(auto *identifier = dynamic_cast<const Syntax::IdentifierExprNode *>(node->rhs); identifier) {
                _chunk->emit<Bytecode::Op::OpCode::Load>(dst, _chunk->addConstant(constVal(identifier->token)));
                _chunk->emit<Bytecode::Op::OpCode::GProp>(reg1, dst, dst);
            } else {
                Bytecode::Register reg2{ 0 };
                if(!expectValue(node->rhs, reg2))
                    return;

                _chunk->emit<Bytecode::Op::OpCode::GProp>(reg1, reg2, dst);
            }
            retReg = dst;
            return;
        }

        if(node->token.type() == Swap) {
            const auto lVarExpr = dynamic_cast<const Syntax::IdentifierExprNode *>(node->lhs);
            const auto rVarExpr = dynamic_cast<const Syntax::IdentifierExprNode *>(node->rhs);
            if(!rVarExpr || !lVarExpr) {
                // TODO: 类的属性替换需要以后添加
                error("isn't support operator", node->location);
                return;
            }
            const Atom lVarName = constVal(lVarExpr->token).value<Atom>();
            const Atom rVarName = constVal(rVarExpr->token).value<Atom>();
            auto *lVar = resolveLocalVariable(lVarName);
            auto *rVar = resolveLocalVariable(rVarName);
            auto dst = allocateRegister();
            OptReg lVarReg{};
            if(lVar) {
                lVarReg = lVar->reg;
            } else {
                lVarReg = allocateRegister();
                _chunk->emit<Bytecode::Op::OpCode::GGlobal>(lVarName, *lVarReg);
            }
            _chunk->emit<Bytecode::Op::OpCode::Mov>(*lVarReg, dst);
            OptReg rVarReg{};

            if(rVar) {
                rVarReg = rVar->reg;
            } else {
                rVarReg = allocateRegister();
                _chunk->emit<Bytecode::Op::OpCode::GGlobal>(rVarName, *rVarReg);
            }

            if(lVar) {
                _chunk->emit<Bytecode::Op::OpCode::Mov>(*rVarReg, *lVarReg);
            } else {
                _chunk->emit<Bytecode::Op::OpCode::DGlobal>(lVarName, *rVarReg);
            }

            if(rVar) {
                _chunk->emit<Bytecode::Op::OpCode::Mov>(dst, rVar->reg);
            } else {
                _chunk->emit<Bytecode::Op::OpCode::DGlobal>(rVarName, dst);
            }
            // retReg = rVarReg;
            return;
        }

        Bytecode::Register src{ 0 };
        if(!expectValue(node->lhs, src))
            return;

        Bytecode::Register dst{ 0 };
        if(!expectValue(node->rhs, dst))
            return;

        if(node->token.type() == Comma) {
            retReg = dst;
            return;
        }

        switch(node->token.type()) {
            case Equal:
                _chunk->emit<Bytecode::Op::OpCode::EQ>(src, dst);
                break;
            case NotEqual:
                _chunk->emit<Bytecode::Op::OpCode::NEQ>(src, dst);
                break;
            case Gt:
                _chunk->emit<Bytecode::Op::OpCode::GT>(src, dst);
                break;
            case GtOrEqual:
                _chunk->emit<Bytecode::Op::OpCode::GE>(src, dst);
                break;
            case Lt:
                _chunk->emit<Bytecode::Op::OpCode::LT>(src, dst);
                break;
            case LtOrEqual:
                _chunk->emit<Bytecode::Op::OpCode::LE>(src, dst);
                break;
            case LogicalAnd:
                _chunk->emit<Bytecode::Op::OpCode::LAnd>(src, dst);
                break;
            case LogicalOr:
                _chunk->emit<Bytecode::Op::OpCode::LOr>(src, dst);
                break;
            case Plus:
                _chunk->emit<Bytecode::Op::OpCode::Add>(src, dst);
                break;
            case Minus:
                _chunk->emit<Bytecode::Op::OpCode::Sub>(src, dst);
                break;
            case Asterisk:
                _chunk->emit<Bytecode::Op::OpCode::Mul>(src, dst);
                break;
            case Slash:
                _chunk->emit<Bytecode::Op::OpCode::Div>(src, dst);
                break;
            case Backslash:
                _chunk->emit<Bytecode::Op::OpCode::Idiv>(src, dst);
                break;
            case Percent:
                _chunk->emit<Bytecode::Op::OpCode::Mod>(src, dst);
                break;
            case Chevron:
                _chunk->emit<Bytecode::Op::OpCode::BXor>(src, dst);
                break;
            case VertLine:
                _chunk->emit<Bytecode::Op::OpCode::BOr>(src, dst);
                break;
            case Ampersand:
                _chunk->emit<Bytecode::Op::OpCode::BAnd>(src, dst);
                break;
            case LArithShift:
                _chunk->emit<Bytecode::Op::OpCode::BLShift>(src, dst);
                break;
            case RArithShift:
                _chunk->emit<Bytecode::Op::OpCode::BRShift>(src, dst);
                break;
            case RBitShift:
                _chunk->emit<Bytecode::Op::OpCode::BURShift>(src, dst);
                break;
            default:
                error("unknow binary operator", node->location);
                return;
        }

        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::UnaryExprNode *node, OptReg &retReg) {
        using enum Syntax::TokenType;
        switch(node->token.type()) {
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
                error("unknow unary operator", node->location);
        }
    }

    void IRGenerator::generate(const Syntax::ProcCallExprNode *node, OptReg &retReg) {
        auto dst = allocateRegister();
        std::vector<Bytecode::Register> arguments{};
        Bytecode::Register memberReg{ 0 };
        if(!expectValue(node->memberAccess, memberReg)) {
            return;
        }

        for(const auto *exprNode : node->arguments) {
            if(!exprNode) {
                _chunk->emit<Bytecode::Op::OpCode::PushReg>(loadVoidReg(*_chunk));
            } else {
                Bytecode::Register reg{ 0 };
                if(!expectValue(exprNode, reg)) {
                    return;
                }
                _chunk->emit<Bytecode::Op::OpCode::PushReg>(reg);
            }
        }
        freeRegister(memberReg);
        _chunk->emit<Bytecode::Op::OpCode::Call>(dst, memberReg, node->arguments.size());
        if(!node->arguments.empty()) {
            _chunk->emit<Bytecode::Op::OpCode::PopN>(node->arguments.size());
        }
        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::AssignExprNode *node, OptReg &retReg) {
        if(const auto *expr = dynamic_cast<const Syntax::IdentifierExprNode *>(node->lhs)) {
            const auto identifier = constVal(expr->token);

            Bytecode::Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;

            if(const auto variable = resolveLocalVariable(identifier.value<Atom>())) {
                Bytecode::Register dst = variable->reg;
                _chunk->emit<Bytecode::Op::OpCode::CP>(src, dst);
                freeRegister(src);
                retReg = dst;
                return;
            }

            // global maybe
            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier.value<Atom>(), src);
            retReg = src;
            return;
        }

        // TODO: member access
        error("isn't support assign operator", node->location);
    }

    void IRGenerator::generate(const Syntax::FunctionExprNode *node, OptReg &retReg) {
        auto funReg = allocateRegister();
        auto *funcMeta = generateFuncMeta(node->parameters, node->body);

        if(!funcMeta) {
            error("generate chunk failed with function expr", node->location);
            return;
        }

        _chunk->emit<Bytecode::Op::OpCode::Load>(funReg, _chunk->addConstant(Constant{ funcMeta }));

        retReg = funReg;
    }

    void IRGenerator::generate(const Syntax::VarDeclNode *node, OptReg &) {
        const auto identifier = constVal(node->token).value<Atom>();

        DEFER {
            OptReg reg;
            if(node->next)
                node->next->generateBytecode(this, reg);
        };

        // can init
        if(node->rhs) {
            Bytecode::Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;

            if(isTopScope()) {
                freeRegister(src);
                _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, src);
                return;
            }

            // already have this variable, in same scope
            if(const auto variable = resolveLocalVariable(identifier)) {
                freeRegister(src);
                _chunk->emit<Bytecode::Op::OpCode::Mov>(src, variable->reg);
                return;
            }

            // not found but can init
            addLocalVar(LocalVariable{ identifier, src, getNextInstPos() });
            return;
        }

        // global
        if(isTopScope()) {
            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, loadVoidReg(*_chunk));
            return;
        }

        // not found and can't init
        addLocalVar(LocalVariable{ identifier, loadVoidReg(*_chunk), getNextInstPos() });
    }

    void IRGenerator::generate(const Syntax::FunctionDeclNode *node, OptReg &) {
        auto funReg = allocateRegister();
        auto *funcMeta = generateFuncMeta(node->parameters, node->body);

        if(!funcMeta) {
            error("generate chunk failed with function declaration", node->location);
            return;
        }


        const auto identifier = constVal(node->token).value<Atom>();

        _chunk->emit<Bytecode::Op::OpCode::Load>(funReg, _chunk->addConstant(Constant{ funcMeta }));

        if(isTopScope()) {
            freeRegister(funReg);
            _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, funReg);
            return;
        }

        addLocalVar(LocalVariable{ identifier, funReg, getNextInstPos() });
    }


    void IRGenerator::generate(const Syntax::ClassDeclNode *node, OptReg &) {
        // TODO:
        const auto identifier = constVal(node->token).value<Atom>();

        auto classReg = allocateRegister();
        auto *classMeta = _rt.createNoGC<ClassMeta>(identifier, 0);
        _chunk->emit<Bytecode::Op::OpCode::Load>(classReg, _chunk->addConstant(Constant{ classMeta }));

        // class is always global in current design
        _chunk->emit<Bytecode::Op::OpCode::DGlobal>(identifier, classReg);

        beginScope();
        for(const auto &declNode : node->body->childrens) {

            if(const auto *funcDeclNode = dynamic_cast<Syntax::FunctionDeclNode *>(declNode)) {
                const auto funName = constVal(funcDeclNode->token).value<Atom>();
                const auto funcMeta = generateFuncMeta(funcDeclNode->parameters, funcDeclNode->body);

                if(!funcMeta) {
                    error("generate chunk failed with class function declaration", node->location);
                    return;
                }
                classMeta->setMember(MemberShapeMeta{ funName }, funcMeta);
            }
            // else if(const auto *varDeclNode = dynamic_cast<Syntax::VarDeclNode *>(declNode)) {
            // TODO:
            // const auto varName = varDeclNode->token.constVal().value<Atom>();
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
            // }
        }

        endScope();
        freeRegister(classReg);
    }

    void IRGenerator::generate(const Syntax::IdentifierExprNode *node, OptReg &retReg) {
        auto dst = allocateRegister();
        const auto identifier = constVal(node->token).value<Atom>();

        if(const auto variable = resolveLocalVariable(identifier)) {
            _chunk->emit<Bytecode::Op::OpCode::CP>(variable->reg, dst);
            retReg = dst;
            return;
        }

        if(isTopScope()) {
            _chunk->emit<Bytecode::Op::OpCode::GGlobal>(identifier, dst);
        } else {
            // dynamic get
            // but tjs2 doesn't support get up function scope local var
            // _chunk->emit<Bytecode::Op::OpCode::GUpval>(identifier, dst);
            _chunk->emit<Bytecode::Op::OpCode::GThis>(identifier, dst);
        }

        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::StmtDeclNode *node, OptReg &retReg) {
        return node->statement->generateBytecode(this, retReg);
    }

    void IRGenerator::generate(const Syntax::BlockStmtNode *node, OptReg &) {
        // block statement never return value
        beginScope();
        OptReg ignore{};
        for(const auto children : node->childrens) {
            children->generateBytecode(this, ignore);
        }
        endScope();
    }

    void IRGenerator::generate(const Syntax::IfStmtNode *node, OptReg &) {
        OptReg ignore{};
        Bytecode::Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg);

        auto *jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();
        node->body->generateBytecode(this, ignore);
        Bytecode::Op::Instruction *jmp = nullptr;

        if(node->elseBody) {
            jmp = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
        }

        Bytecode::Op::JmpNE::setTarget(*jmpNE, makeLabel());

        if(node->elseBody) {
            node->elseBody->generateBytecode(this, ignore);
            Bytecode::Op::Jmp::setTarget(*jmp, makeLabel());
        }

        freeRegister(testReg);
    }

    void IRGenerator::generate(const Syntax::SwitchStmtNode *node, OptReg &) {
        using namespace Bytecode;

        _breakStack.emplace_back();

        DEFER { _breakStack.pop_back(); };

        OptReg ignore{ 0 };

        Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }
        Op::Instruction *prevCaseJmp{};
        for(auto &[matchExprVec, body] : node->matches) {
            Vec<Op::Instruction *> jmpEVec{};
            for(const auto *matchExpr : matchExprVec) {
                Register matchReg{ 0 };

                if(!expectValue(matchExpr, matchReg)) {
                    return;
                }
                _chunk->emit<Op::OpCode::EQ>(testReg, matchReg);
                jmpEVec.push_back(_chunk->emit<Op::OpCode::JmpE>());
            }
            auto *jmp = _chunk->emit<Op::OpCode::Jmp>();
            for(auto *jmpE : jmpEVec) {
                Op::JmpE::setTarget(*jmpE, makeLabel());
            }

            if(prevCaseJmp)
                Op::Jmp::setTarget(*prevCaseJmp, makeLabel());

            body->generateBytecode(this, ignore);

            if(body != node->matches.back().second)
                prevCaseJmp = _chunk->emit<Op::OpCode::Jmp>();

            Op::Jmp::setTarget(*jmp, makeLabel());
        }

        if(node->defaultBody)
            node->defaultBody->generateBytecode(this, ignore);

        const auto exitLabel = makeLabel();

        for(auto *br : _breakStack.back()) {
            Op::Jmp::setTarget(*br, exitLabel);
        }
    }

    void IRGenerator::generate(const Syntax::DoWhileStmtNode *node, OptReg &) {
        _breakStack.emplace_back();
        _continueStack.emplace_back();

        DEFER {
            _breakStack.pop_back();
            _continueStack.pop_back();
        };

        OptReg ignore{};
        const auto bodyLabel = makeLabel();

        node->body->generateBytecode(this, ignore);

        const auto testLabel = makeLabel();
        _continueStack.back().continueLabel = testLabel;
        for(auto *ct : _continueStack.back().continues) {
            Bytecode::Op::Jmp::setTarget(*ct, testLabel);
        }

        Bytecode::Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg);
        auto *jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();
        Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(), bodyLabel);

        const auto exitLabel = makeLabel();
        Bytecode::Op::JmpNE::setTarget(*jmpNE, exitLabel);
        for(auto *br : _breakStack.back()) {
            Bytecode::Op::Jmp::setTarget(*br, exitLabel);
        }

        freeRegister(testReg);
    }

    void IRGenerator::generate(const Syntax::ForStmtNode *node, OptReg &) {
        _breakStack.emplace_back();
        _continueStack.emplace_back();

        DEFER {
            _breakStack.pop_back();
            _continueStack.pop_back();
        };
        OptReg ignore{};
        if(node->init)
            node->init->generateBytecode(this, ignore);

        const auto testLabel = makeLabel();

        Bytecode::Op::Instruction *jmpNE{ nullptr };
        Bytecode::Register testReg{ 0 };
        if(node->test) {
            if(!expectValue(node->test, testReg)) {
                return;
            }
            _chunk->emit<Bytecode::Op::OpCode::Test>(testReg);
            jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();
        }

        node->body->generateBytecode(this, ignore);

        const auto stepLabel = makeLabel();
        _continueStack.back().continueLabel = stepLabel;
        for(auto *ct : _continueStack.back().continues) {
            Bytecode::Op::Jmp::setTarget(*ct, stepLabel);
        }

        if(node->step) {
            Bytecode::Register stepReg{ 0 };
            if(!expectValue(node->step, stepReg)) {
                return;
            }
            freeRegister(stepReg);
        }

        Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(), testLabel);

        const auto exitLabel = makeLabel();
        if(jmpNE)
            Bytecode::Op::JmpNE::setTarget(*jmpNE, exitLabel);
        for(auto *br : _breakStack.back()) {
            Bytecode::Op::Jmp::setTarget(*br, exitLabel);
        }

        if(node->test)
            freeRegister(testReg);
    }

    void IRGenerator::generate(const Syntax::WhileStmtNode *node, OptReg &) {
        _breakStack.emplace_back();
        _continueStack.emplace_back();

        DEFER {
            _breakStack.pop_back();
            _continueStack.pop_back();
        };

        OptReg ignore{};
        const auto loopLabel = makeLabel();

        Bytecode::Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg);
        auto *jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();

        node->body->generateBytecode(this, ignore);

        const auto testLabel = makeLabel();
        _continueStack.back().continueLabel = testLabel;
        for(auto *ct : _continueStack.back().continues) {
            Bytecode::Op::Jmp::setTarget(*ct, testLabel);
        }

        Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(), loopLabel);
        const auto exitLabel = makeLabel();
        Bytecode::Op::JmpNE::setTarget(*jmpNE, exitLabel);

        for(auto *br : _breakStack.back()) {
            Bytecode::Op::Jmp::setTarget(*br, exitLabel);
        }

        freeRegister(testReg);
    }

    void IRGenerator::generate(const Syntax::BreakStmtNode *, OptReg &) {
        if(_breakStack.empty())
            return;
        auto *itt = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
        _breakStack.back().push_back(itt);
    }

    void IRGenerator::generate(const Syntax::ContinueStmtNode *, OptReg &) {
        if(_continueStack.empty())
            return;
        if(_continueStack.back().continueLabel.has_value()) {
            Bytecode::Op::Jmp::setTarget(*_chunk->emit<Bytecode::Op::OpCode::Jmp>(),
                                         _continueStack.back().continueLabel.value());
        } else {
            auto *itt = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
            _continueStack.back().continues.push_back(itt);
        }
    }

    void IRGenerator::generate(const Syntax::ConditionalTernaryExprNode *node, OptReg &retReg) {
        Bytecode::Register dst = allocateRegister();
        Bytecode::Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<Bytecode::Op::OpCode::Test>(testReg);
        auto *jmpNE = _chunk->emit<Bytecode::Op::OpCode::JmpNE>();

        Bytecode::Register lhsReg{ 0 };
        if(!expectValue(node->lhsExpr, lhsReg)) {
            return;
        }

        _chunk->emit<Bytecode::Op::OpCode::Mov>(lhsReg, dst);

        auto *jmp = _chunk->emit<Bytecode::Op::OpCode::Jmp>();
        Bytecode::Op::JmpNE::setTarget(*jmpNE, makeLabel());

        Bytecode::Register rhsReg{ 0 };
        if(!expectValue(node->rhsExpr, rhsReg)) {
            return;
        }

        _chunk->emit<Bytecode::Op::OpCode::Mov>(rhsReg, dst);

        Bytecode::Op::Jmp::setTarget(*jmp, makeLabel());
        freeRegister(testReg);
        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::ReturnStmtNode *node, OptReg &) {
        if(node->expr) {
            Bytecode::Register reg{ 0 };
            if(!expectValue(node->expr, reg)) {
                return;
            }
            _chunk->emit<Bytecode::Op::OpCode::Ret>(reg);
            return;
        }
        _chunk->emit<Bytecode::Op::OpCode::Ret>(loadVoidReg(*_chunk));
    }

    LocalVariable *IRGenerator::resolveLocalVariable(const Atom identifier) {
        for(auto &var : std::ranges::reverse_view(_localVars)) {
            if(var.endPC == 0 && var.identifier.v == identifier.v) {
                return &var;
            }
        }
        return nullptr;
    }

    FuncMeta *IRGenerator::generateFuncMeta(const Syntax::Parameters &parameters,
                                            const Syntax::BlockStmtNode *body) const {

        auto gen = IRGenerator{ _rt, _sourceFile };
        gen.makeVirtualGlobalScope();

        OptReg paramReg{};

        for(auto &[token, exprNode] : parameters) {
            const auto varName = constVal(token).value<Atom>();

            if(exprNode) {
                Bytecode::Register defaultParamReg{ 0 };
                if(!gen.expectValue(exprNode, defaultParamReg)) {
                    return nullptr;
                }
                paramReg = defaultParamReg;
            } else {
                paramReg = gen.allocateRegister();
            }
            const auto startPC = gen.getNextInstPos();
            gen.addLocalVar(LocalVariable{ varName, paramReg.value(), startPC });
        }

        OptReg ignoreReg{};
        auto funChunk = gen.parseAst(_r, body, ignoreReg);
        assert(funChunk);

        // the last instruction is not ret, patch one ret
        if(auto &instVec = funChunk->getInstVec();
           instVec.empty() || instVec.back()->opcode != Bytecode::Op::OpCode::Ret) {
            funChunk->emit<Bytecode::Op::OpCode::Ret>(gen.loadVoidReg(*funChunk));
        }

        auto *chunk = _rt.createNoGC<Bytecode::Chunk>(std::move(*funChunk));
        return _rt.createNoGC<FuncMeta>(static_cast<std::uint32_t>(parameters.size()), chunk,
                                        std::move(gen._localVars));
    }

    Bytecode::Register IRGenerator::loadVoidReg(Bytecode::Chunk &chunk) {
        if(!_empty.has_value()) {
            _empty = allocateRegister();
            chunk.emit<Bytecode::Op::OpCode::Load>(_empty.value(), ConstIdx{ 0 });
        }
        return _empty.value();
    }

    Constant IRGenerator::constVal(const Syntax::Token &token) const {
        switch(token.valueType()) {
            case Syntax::TokenValueType::Integer:
                return Constant{ token.getInteger() };
            case Syntax::TokenValueType::Real:
                return Constant{ token.getReal() };
            case Syntax::TokenValueType::String:
                return Constant{ _rt.atomTable.intern(token.getString()) };
            case Syntax::TokenValueType::Octet:
                throw std::runtime_error("Unsupported token type octet");
                // return Constant { token.getOctet() };
            case Syntax::TokenValueType::None:
                break;
        }
        return Constant{};
    }

} // namespace cial::Inter
