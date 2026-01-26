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

#include "Optimizer.hpp"
#include "common/Defer.hpp"
#include "runtime/Runtime.hpp"

#include "parser/ast/DeclNode.hpp"
#include "parser/ast/ExprNode.hpp"
#include "parser/ast/StmtNode.hpp"

#include "Instruction.hpp"

namespace cial::Inter {

    Opt<Bytecode::Chunk> IRGenerator::parseAst(const Syntax::AstNode *node, OptReg &retReg) {
        if(node)
            node->generateBytecode(this, retReg);
        if(_r.isFailed())
            return {};
        _chunk->setRegCount(_regNextIndex);
        auto chunk = std::move(_chunk);

        // ===
        // OptimizerManager optimizerManager(*chunk);
        // optimizerManager.addOptimizer(std::make_unique<LoadSubOptimizer>());
        // optimizerManager.applyOptimizations();
        // ===

        _chunk = std::make_unique<Bytecode::Chunk>();
        return Bytecode::Chunk{ std::move(*chunk.release()) };
    }

    bool IRGenerator::expectValue(const Syntax::ExprNode *node, Register &ret) {
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

    void IRGenerator::generate(const Syntax::TryStmtNode *node, OptReg &retReg) {
        const auto tryStartIp = makeLabel(); // closed interval
        node->tryBlock->generateBytecode(this, retReg);

        size_t jmpIdx = _chunk->emit<OpCode::Jmp>();

        const auto tryEndIp = makeLabel(); // open interval (same as catch start ip closed interval)

        if(node->catchErr) {
            beginScope();

            const Atom varName = getAtomFromToken(node->catchErr->token);
            const Register exValue = allocateRegister();
            addLocalVar(LocalVariable{ varName, exValue, tryEndIp });

            node->catchBlock->generateBytecode(this, retReg);
            Jmp::target(_chunk->inst(jmpIdx), makeLabel());

            _chunk->addThrowHandler({ tryStartIp.address(), tryEndIp.address(), exValue.index() });
            endScope();
        } else {
            node->catchBlock->generateBytecode(this, retReg);
            Jmp::target(_chunk->inst(jmpIdx), makeLabel());
            _chunk->addThrowHandler({ tryStartIp.address(), tryEndIp.address() });
        }

        retReg = {};
    }

    void IRGenerator::generate(const Syntax::ValueExprNode *node, OptReg &retReg) {
        if(node->token.type() == Syntax::TokenType::Null) {
            error("null token current not support", node->location);
            return;
        }
        auto dst = allocateRegister();
        genTokenValueLoadInst(dst, node->token);
        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::BinaryExprNode *node, OptReg &retReg) {
        using enum Syntax::TokenType;

        if(node->token.type() == Dot) {
            Register reg1{ 0 };
            if(!expectValue(node->lhs, reg1))
                return;

            Register dst = allocateRegister();

            if(auto *identifier = dynamic_cast<const Syntax::IdentifierExprNode *>(node->rhs); identifier) {
                genTokenValueLoadInst(dst, identifier->token);
                _chunk->emit<OpCode::GProp>(reg1, dst, dst);
            } else {
                Register reg2{ 0 };
                if(!expectValue(node->rhs, reg2))
                    return;

                _chunk->emit<OpCode::GProp>(reg1, reg2, dst);
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
            const Atom lVarName = getAtomFromToken(lVarExpr->token);
            const Atom rVarName = getAtomFromToken(rVarExpr->token);
            auto *lVar = resolveLocalVariable(lVarName);
            auto *rVar = resolveLocalVariable(rVarName);
            auto dst = allocateRegister();
            OptReg lVarReg{};
            if(lVar) {
                lVarReg = lVar->reg;
            } else {
                lVarReg = allocateRegister();
                _chunk->emit<OpCode::GGlobal>(lVarName, *lVarReg);
            }
            _chunk->emit<OpCode::Mov>(*lVarReg, dst);
            OptReg rVarReg{};

            if(rVar) {
                rVarReg = rVar->reg;
            } else {
                rVarReg = allocateRegister();
                _chunk->emit<OpCode::GGlobal>(rVarName, *rVarReg);
            }

            if(lVar) {
                _chunk->emit<OpCode::Mov>(*rVarReg, *lVarReg);
            } else {
                _chunk->emit<OpCode::DGlobal>(lVarName, *rVarReg);
            }

            if(rVar) {
                _chunk->emit<OpCode::Mov>(dst, rVar->reg);
            } else {
                _chunk->emit<OpCode::DGlobal>(rVarName, dst);
            }
            // retReg = rVarReg;
            return;
        }

        Register src{ 0 };
        if(!expectValue(node->lhs, src))
            return;

        Register dst{ 0 };
        if(!expectValue(node->rhs, dst))
            return;

        if(node->token.type() == Comma) {
            retReg = dst;
            return;
        }

        switch(node->token.type()) {
            case Equal:
                _chunk->emit<OpCode::EQ>(src, dst);
                break;
            case NotEqual:
                _chunk->emit<OpCode::NEQ>(src, dst);
                break;
            case DiscEqual:
                _chunk->emit<OpCode::AbsEQ>(src, dst);
                break;
            case DiscNotEqual:
                _chunk->emit<OpCode::AbsNEQ>(src, dst);
                break;
            case Gt:
                _chunk->emit<OpCode::GT>(src, dst);
                break;
            case GtOrEqual:
                _chunk->emit<OpCode::GE>(src, dst);
                break;
            case Lt:
                _chunk->emit<OpCode::LT>(src, dst);
                break;
            case LtOrEqual:
                _chunk->emit<OpCode::LE>(src, dst);
                break;
            case LogicalAnd:
                _chunk->emit<OpCode::LAnd>(src, dst);
                break;
            case LogicalOr:
                _chunk->emit<OpCode::LOr>(src, dst);
                break;
            case Plus:
                _chunk->emit<OpCode::Add>(src, dst);
                break;
            case Minus:
                _chunk->emit<OpCode::Sub>(src, dst);
                break;
            case Asterisk:
                _chunk->emit<OpCode::Mul>(src, dst);
                break;
            case Slash:
                _chunk->emit<OpCode::Div>(src, dst);
                break;
            case Backslash:
                _chunk->emit<OpCode::Idiv>(src, dst);
                break;
            case Percent:
                _chunk->emit<OpCode::Mod>(src, dst);
                break;
            case Chevron:
                _chunk->emit<OpCode::BXor>(src, dst);
                break;
            case VertLine:
                _chunk->emit<OpCode::BOr>(src, dst);
                break;
            case Ampersand:
                _chunk->emit<OpCode::BAnd>(src, dst);
                break;
            case LArithShift:
                _chunk->emit<OpCode::BlShift>(src, dst);
                break;
            case RArithShift:
                _chunk->emit<OpCode::BrShift>(src, dst);
                break;
            case RBitShift:
                _chunk->emit<OpCode::BurShift>(src, dst);
                break;
            case InContextOf:
                _chunk->emit<OpCode::ChgThis>(src, dst);
                retReg = src;
                return;
            case Instanceof:
                _chunk->emit<OpCode::ChkIns>(src, dst);
                retReg = src;
                return;
            default:
                error("unknow binary operator", node->location);
                return;
        }

        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::PrefixUnaryExprNode *node, OptReg &retReg) {
        using enum Syntax::TokenType;
        switch(node->token.type()) {
            case Throw: {
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<OpCode::Throw>(reg);
                // retReg = reg;
                break;
            }
            case New:
                node->rhs->generateBytecode(this, retReg);
                break;
            case Exclamation:
                node->rhs->generateBytecode(this, retReg);
                _chunk->emit<OpCode::LNot>(*retReg);
                break;
            case Minus:
                node->rhs->generateBytecode(this, retReg);
                _chunk->emit<OpCode::ChgSign>(*retReg);
                break;
            case Invalidate:
                node->rhs->generateBytecode(this, retReg);
                _chunk->emit<OpCode::Inv>(*retReg);
                break;
            case Isvalid: {
                Register reg{};
                auto dst = allocateRegister();
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<OpCode::ChkInv>(reg, dst);
                retReg = dst;
                break;
            }
            case Int: {
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<OpCode::ToInt>(reg);
                retReg = reg;
                break;
            }
            case Real: {
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<OpCode::ToReal>(reg);
                retReg = reg;
                break;
            }
            case String: {
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<OpCode::ToString>(reg);
                retReg = reg;
                break;
            }
            default:
                error("unknow prefix unary operator", node->location);
        }
    }


    void IRGenerator::generate(const Syntax::SuffixUnaryExprNode *node, OptReg &retReg) {
        // TODO:
        using enum Syntax::TokenType;
        switch(node->token.type()) {
            case Isvalid: {
                auto src = allocateRegister();
                if(!expectValue(node->lhs, src)) {
                    return;
                }
                _chunk->emit<OpCode::ChkInv>(src, *retReg);
                freeRegister(src);
                break;
            }
            case Increment: {
                if(const auto *expr = dynamic_cast<const Syntax::IdentifierExprNode *>(node->lhs)) {
                    const Atom identifier = getAtomFromToken(expr->token);

                    if(const auto variable = resolveLocalVariable(identifier)) {
                        Register dst = variable->reg;
                        auto tmpR = allocateRegister();
                        _chunk->emit<OpCode::ILoad>(tmpR, 1);
                        _chunk->emit<OpCode::Add>(tmpR, dst);
                        freeRegister(tmpR);
                        retReg = dst;
                        return;
                    }

                    auto tmpR2 = allocateRegister();
                    auto tmpR1 = allocateRegister();
                    if(isTopScope()) {
                        // global maybe
                        _chunk->emit<OpCode::ILoad>(tmpR1, 1);
                        _chunk->emit<OpCode::GGlobal>(identifier, tmpR2);
                        _chunk->emit<OpCode::Add>(tmpR1, tmpR2);
                        _chunk->emit<OpCode::DGlobal>(identifier, tmpR2);
                        retReg = tmpR2;
                    } else {
                        // find on context
                        _chunk->emit<OpCode::ILoad>(tmpR1, 1);
                        _chunk->emit<OpCode::GThis>(identifier, tmpR2);
                        _chunk->emit<OpCode::Add>(tmpR1, tmpR2);
                        _chunk->emit<OpCode::DThis>(identifier, tmpR2);
                    }
                    freeRegister(tmpR1);

                    retReg = tmpR2;
                    return;
                }
                error("current not support dot chain call", node->location);
                return;
            }
            case Decrement: {

                if(const auto *expr = dynamic_cast<const Syntax::IdentifierExprNode *>(node->lhs)) {
                    const Atom identifier = getAtomFromToken(expr->token);

                    if(const auto variable = resolveLocalVariable(identifier)) {
                        Register dst = variable->reg;
                        auto tmpR = allocateRegister();
                        _chunk->emit<OpCode::ILoad>(tmpR, 1);
                        _chunk->emit<OpCode::Sub>(tmpR, dst);
                        freeRegister(tmpR);
                        retReg = dst;
                        return;
                    }

                    auto tmpR2 = allocateRegister();
                    auto tmpR1 = allocateRegister();
                    if(isTopScope()) {
                        // global maybe
                        _chunk->emit<OpCode::ILoad>(tmpR1, 1);
                        _chunk->emit<OpCode::GGlobal>(identifier, tmpR2);
                        _chunk->emit<OpCode::Sub>(tmpR1, tmpR2);
                        _chunk->emit<OpCode::DGlobal>(identifier, tmpR2);
                        retReg = tmpR2;
                    } else {
                        // find on context
                        _chunk->emit<OpCode::ILoad>(tmpR1, 1);
                        _chunk->emit<OpCode::GThis>(identifier, tmpR2);
                        _chunk->emit<OpCode::Sub>(tmpR1, tmpR2);
                        _chunk->emit<OpCode::DThis>(identifier, tmpR2);
                    }
                    freeRegister(tmpR1);

                    retReg = tmpR2;
                    return;
                }
                error("current not support dot chain call", node->location);
                return;
            }
            default:
                error("unknow suffix unary operator", node->location);
        }
    }

    void IRGenerator::generate(const Syntax::ProcCallExprNode *node, OptReg &retReg) {
        auto dst = allocateRegister();
        std::vector<Register> arguments{};
        Register memberReg{ 0 };
        if(!expectValue(node->memberAccess, memberReg)) {
            return;
        }

        for(const auto *exprNode : node->arguments) {
            if(!exprNode) {
                _chunk->emit<OpCode::Push>(loadVoidReg());
            } else {
                Register reg{ 0 };
                if(!expectValue(exprNode, reg)) {
                    return;
                }
                _chunk->emit<OpCode::Push>(reg);
            }
        }
        freeRegister(memberReg);
        _chunk->emit<OpCode::Call>(dst, memberReg, static_cast<Integer>(node->arguments.size()));
        // if(!node->arguments.empty()) {
        //     _chunk->emit<OpCode::PopN>(node->arguments.size());
        // }
        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::AssignExprNode *node, OptReg &retReg) {
        if(const auto *expr = dynamic_cast<const Syntax::IdentifierExprNode *>(node->lhs)) {
            const Atom identifier = getAtomFromToken(expr->token);

            Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;

            if(const auto variable = resolveLocalVariable(identifier)) {
                Register dst = variable->reg;
                _chunk->emit<OpCode::Mov>(src, dst);
                freeRegister(src);
                retReg = dst;
                return;
            }

            if(isTopScope()) {
                // global maybe
                _chunk->emit<OpCode::DGlobal>(identifier, src);
            } else {
                // find on context
                _chunk->emit<OpCode::DThis>(identifier, src);
            }

            retReg = src;
            return;
        }

        if(const auto *expr = dynamic_cast<const Syntax::BinaryExprNode *>(node->lhs)) {

            Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;

            if(expr->token.type() == Syntax::TokenType::Dot) {

                Register lhsR{ 0 };
                if(!expectValue(expr->lhs, lhsR))
                    return;

                if(const auto *identifierExpr = dynamic_cast<const Syntax::IdentifierExprNode *>(expr->rhs)) {
                    auto tmpR = allocateRegister();
                    genTokenValueLoadInst(tmpR, identifierExpr->token);
                    _chunk->emit<OpCode::DProp>(lhsR, tmpR, src);
                    freeRegister(tmpR);

                    retReg = src;
                    return;
                }
            }
        }

        error("isn't support assign operator", node->location);
    }

    void IRGenerator::generate(const Syntax::FunctionExprNode *node, OptReg &retReg) {
        auto funReg = allocateRegister();
        auto *funcMeta = generateFuncMeta(node->parameters, node->body);

        if(!funcMeta) {
            error("generate chunk failed with function expr", node->location);
            return;
        }

        _chunk->emit<OpCode::Load>(funReg, _chunk->addConstant(funcMeta));

        retReg = funReg;
    }

    void IRGenerator::generate(const Syntax::PropertyDeclNode *node, OptReg &) {
        FuncMeta *setFuncMeta{};
        if(node->setter)
            setFuncMeta = generateFuncMeta(node->setter->parameters, node->setter->body);

        FuncMeta *getFuncMeta;
        if(node->getter)
            getFuncMeta = generateFuncMeta(node->getter->parameters, node->getter->body);

        auto *propMeta = _rt.createNoGC<PropMeta>(setFuncMeta, getFuncMeta);

        const auto identifier = getAtomFromToken(node->token);

        auto propReg = allocateRegister();
        _chunk->emit<OpCode::Load>(propReg, _chunk->addConstant(propMeta));

        if(isTopScope()) {
            freeRegister(propReg);
            _chunk->emit<OpCode::DGlobal>(identifier, propReg);
            return;
        }

        addLocalVar(LocalVariable{ identifier, propReg, makeLabel() });
    }

    void IRGenerator::generate(const Syntax::VarDeclNode *node, OptReg &) {
        const auto identifier = getAtomFromToken(node->token);

        DEFER {
            OptReg reg;
            if(node->next)
                node->next->generateBytecode(this, reg);
        };

        // can init
        if(node->rhs) {
            Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;

            if(isTopScope()) {
                freeRegister(src);
                _chunk->emit<OpCode::DGlobal>(identifier, src);
                return;
            }

            // already have this variable, in same scope
            if(const auto variable = resolveLocalVariable(identifier)) {
                freeRegister(src);
                _chunk->emit<OpCode::Mov>(src, variable->reg);
                return;
            }

            // not found but can init
            addLocalVar(LocalVariable{ identifier, src, makeLabel() });
            return;
        }

        // global
        if(isTopScope()) {
            _chunk->emit<OpCode::DGlobal>(identifier, loadVoidReg());
            return;
        }

        // not found and can't init
        addLocalVar(LocalVariable{ identifier, loadVoidReg(), makeLabel() });
    }

    void IRGenerator::generate(const Syntax::FunctionDeclNode *node, OptReg &) {
        auto funReg = allocateRegister();
        auto *funcMeta = generateFuncMeta(node->parameters, node->body);

        if(!funcMeta) {
            error("generate chunk failed with function declaration", node->location);
            return;
        }

        const auto identifier = getAtomFromToken(node->token);
        funcMeta->name = identifier;
        _chunk->emit<OpCode::Load>(funReg, _chunk->addConstant(funcMeta));

        if(isTopScope()) {
            freeRegister(funReg);
            _chunk->emit<OpCode::DGlobal>(identifier, funReg);
            return;
        }

        addLocalVar(LocalVariable{ identifier, funReg, makeLabel() });
    }


    void IRGenerator::generate(const Syntax::ClassDeclNode *node, OptReg &) {
        const auto identifier = getAtomFromToken(node->token);

        auto classReg = allocateRegister();
        auto *classMeta = _rt.createNoGC<ClassMeta>(identifier, 0);
        _chunk->emit<OpCode::Load>(classReg, _chunk->addConstant(classMeta));

        // class is always global in current design
        _chunk->emit<OpCode::DGlobal>(identifier, classReg);

        beginScope();

        for(const auto *funcDeclNode : node->funcDeclVec) {
            const auto funName = getAtomFromToken(funcDeclNode->token);
            const auto funcMeta = generateFuncMeta(funcDeclNode->parameters, funcDeclNode->body);

            if(!funcMeta) {
                error("generate chunk failed with class function declaration", node->location);
                return;
            }
            classMeta->setMember(MemberShapeMeta{ .name = funName, .isMethod = true }, ClassFieldMeta{ funcMeta });
        }

        for(const auto *ext : node->extends) {
            classMeta->extends.push_back(getAtomFromToken(ext->token));
        }

        for(const auto *propertyDeclNode : node->propertyDeclVec) {
            FuncMeta *setFuncMeta{};
            if(propertyDeclNode->setter)
                setFuncMeta = generateFuncMeta(propertyDeclNode->setter->parameters, propertyDeclNode->setter->body);

            FuncMeta *getFuncMeta;
            if(propertyDeclNode->getter)
                getFuncMeta = generateFuncMeta(propertyDeclNode->getter->parameters, propertyDeclNode->getter->body);

            auto *propMeta = _rt.createNoGC<PropMeta>(setFuncMeta, getFuncMeta);

            const auto varName = getAtomFromToken(propertyDeclNode->token);

            classMeta->setMember(MemberShapeMeta{ .name = varName, .isProp = true }, ClassFieldMeta{ propMeta });
        }

        {
            auto gen = IRGenerator{ _r, _rt, _sourceFile };
            gen.makeVirtualGlobalScope();

            for(const auto *varDeclNode : node->varDeclVec) {
                const auto varName = getAtomFromToken(varDeclNode->token);

                // can init
                if(varDeclNode->rhs) {
                    Register defaultVarReg{ 0 };
                    if(!gen.expectValue(varDeclNode->rhs, defaultVarReg)) {
                        return;
                    }
                    gen._chunk->emit<OpCode::DThis>(varName, defaultVarReg);
                }

                classMeta->setMember(MemberShapeMeta{ .name = varName, .isVar = true }, ClassFieldMeta{});
            }
            OptReg ignoreReg{};
            auto funChunk = gen.parseAst(nullptr, ignoreReg);
            assert(funChunk);

            auto *chunk = _rt.createNoGC<Bytecode::Chunk>(std::move(*funChunk));
            auto *initFuncMeta = _rt.createNoGC<FuncMeta>(0, chunk, std::move(gen._localVars));
            classMeta->initDefaultVal = initFuncMeta;
        }

        {
            auto gen = IRGenerator{ _r, _rt, _sourceFile };
            gen.makeVirtualGlobalScope();

            // must init var in begin state
            if(node->constructor) {
                for(auto &[token, exprNode] : node->constructor->parameters) {
                    const auto varName = getAtomFromToken(token);
                    OptReg paramReg{};

                    if(exprNode) {
                        Register defaultParamReg{ 0 };
                        if(!gen.expectValue(exprNode, defaultParamReg)) {
                            return;
                        }
                        paramReg = defaultParamReg;
                    } else {
                        paramReg = gen.allocateRegister();
                    }
                    gen.addLocalVar(LocalVariable{ varName, paramReg.value(), gen.makeLabel() });
                }
            }


            if(node->constructor) {
                OptReg ignoreReg{};
                auto funChunk = gen.parseAst(node->constructor->body, ignoreReg);
                assert(funChunk);

                // the last patch one ret
                auto voidReg = gen.loadVoidReg();
                funChunk->emit<OpCode::Ret>(voidReg);

                auto *chunk = _rt.createNoGC<Bytecode::Chunk>(std::move(*funChunk));
                const size_t paramCount = node->constructor->parameters.size();
                auto *initFuncMeta =
                    _rt.createNoGC<FuncMeta>(static_cast<u32>(paramCount), chunk, std::move(gen._localVars));
                initFuncMeta->name = identifier;
                classMeta->setConstructor(initFuncMeta);
            }
        }

        // finalize function placeholder
        if(Atom finalizeAtom = _rt.atomTable.intern("finalize"_str); classMeta->hasMember(finalizeAtom) < 0) {
            OptReg ignoreReg{};
            IRGenerator genFinalize{ _r, _rt, _sourceFile };
            genFinalize.makeVirtualGlobalScope();
            auto funChunk = _rt.createNoGC<Bytecode::Chunk>(*genFinalize.parseAst(nullptr, ignoreReg));

            auto voidReg = genFinalize.loadVoidReg();
            funChunk->emit<OpCode::Ret>(voidReg);

            classMeta->setMember(MemberShapeMeta{ .name = finalizeAtom, .isMethod = true },
                                 ClassFieldMeta{ _rt.createNoGC<FuncMeta>(0, funChunk, Vec<LocalVariable>{}) });
        }
        endScope();
        freeRegister(classReg);
    }

    void IRGenerator::generate(const Syntax::IdentifierExprNode *node, OptReg &retReg) {
        auto dst = allocateRegister();
        const auto identifier = getAtomFromToken(node->token);

        if(const auto variable = resolveLocalVariable(identifier)) {
            _chunk->emit<OpCode::CP>(variable->reg, dst);
            retReg = dst;
            return;
        }

        if(isTopScope()) {
            _chunk->emit<OpCode::GGlobal>(identifier, dst);
        } else {
            // dynamic get
            // but tjs2 doesn't support get up function scope local var
            // _chunk->emit<OpCode::GUpval>(identifier, dst);
            // find on context
            _chunk->emit<OpCode::GThis>(identifier, dst);
        }

        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::InternalIdentifierExprNode *node, OptReg &retReg) {
        auto dst = allocateRegister();

        switch(node->token.type()) {
            case Syntax::TokenType::Global:
                _chunk->emit<OpCode::Global>(dst);
                break;
            case Syntax::TokenType::Super:
                _chunk->emit<OpCode::Super>(dst);
                break;
            case Syntax::TokenType::This:
                _chunk->emit<OpCode::This>(dst);
                break;
            default:
                error("not support", node->location);
                return;
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
        Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<OpCode::Test>(testReg);

        const size_t jmpNeIdx = _chunk->emit<OpCode::JmpNE>();
        node->body->generateBytecode(this, ignore);
        size_t jmpIdx{};

        if(node->elseBody) {
            jmpIdx = _chunk->emit<OpCode::Jmp>();
        }

        Jmp::target(_chunk->inst(jmpNeIdx), makeLabel());

        if(node->elseBody) {
            node->elseBody->generateBytecode(this, ignore);
            Jmp::target(_chunk->inst(jmpIdx), makeLabel());
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

        size_t prevCaseJmpIdx{};
        const size_t matchLen = node->matches.size();
        for(size_t i = 0; i < matchLen; ++i) {
            auto &[matchExprVec, body] = node->matches[i];
            Vec<size_t> jmpEIdxVec{};
            for(const auto *matchExpr : matchExprVec) {
                Register matchReg{ 0 };

                if(!expectValue(matchExpr, matchReg))
                    return;

                _chunk->emit<OpCode::EQ>(testReg, matchReg);
                _chunk->emit<OpCode::Test>(matchReg);
                jmpEIdxVec.push_back(_chunk->emit<OpCode::JmpE>());
            }

            const size_t jmpIdx = _chunk->emit<OpCode::Jmp>();
            for(const size_t jmpEIdx : jmpEIdxVec) {
                Jmp::target(_chunk->inst(jmpEIdx), makeLabel());
            }

            if(i != 0)
                Jmp::target(_chunk->inst(prevCaseJmpIdx), makeLabel());

            body->generateBytecode(this, ignore);

            if(body != node->matches.back().second)
                prevCaseJmpIdx = _chunk->emit<OpCode::Jmp>();

            Jmp::target(_chunk->inst(jmpIdx), makeLabel());
        }

        if(node->defaultBody)
            node->defaultBody->generateBytecode(this, ignore);

        const auto exitLabel = makeLabel();

        for(const size_t br : _breakStack.back()) {
            Jmp::target(_chunk->inst(br), exitLabel);
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
        for(const size_t idx : _continueStack.back().continues) {
            Jmp::target(_chunk->inst(idx), testLabel);
        }

        Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<OpCode::Test>(testReg);
        const size_t jmpNEIdx = _chunk->emit<OpCode::JmpNE>();
        Jmp::target(_chunk->inst(_chunk->emit<OpCode::Jmp>()), bodyLabel);

        const auto exitLabel = makeLabel();
        Jmp::target(_chunk->inst(jmpNEIdx), exitLabel);
        for(const size_t idx : _breakStack.back()) {
            Jmp::target(_chunk->inst(idx), exitLabel);
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

        size_t jmpNeIdx{};
        Register testReg{ 0 };
        if(node->test) {
            if(!expectValue(node->test, testReg))
                return;
            _chunk->emit<OpCode::Test>(testReg);
            jmpNeIdx = _chunk->emit<OpCode::JmpNE>();
        }

        node->body->generateBytecode(this, ignore);

        const auto stepLabel = makeLabel();
        _continueStack.back().continueLabel = stepLabel;
        for(const size_t idx : _continueStack.back().continues) {
            Jmp::target(_chunk->inst(idx), stepLabel);
        }

        if(node->step) {
            Register stepReg{ 0 };
            if(!expectValue(node->step, stepReg)) {
                return;
            }
            freeRegister(stepReg);
        }

        Jmp::target(_chunk->inst(_chunk->emit<OpCode::Jmp>()), testLabel);

        const auto exitLabel = makeLabel();
        if(node->test)
            Jmp::target(_chunk->inst(jmpNeIdx), exitLabel);
        for(const size_t idx : _breakStack.back()) {
            Jmp::target(_chunk->inst(idx), exitLabel);
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

        Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<OpCode::Test>(testReg);
        const size_t jmpNeIdx = _chunk->emit<OpCode::JmpNE>();

        node->body->generateBytecode(this, ignore);

        const auto testLabel = makeLabel();
        _continueStack.back().continueLabel = testLabel;
        for(const size_t idx : _continueStack.back().continues) {
            Jmp::target(_chunk->inst(idx), testLabel);
        }

        Jmp::target(_chunk->inst(_chunk->emit<OpCode::Jmp>()), loopLabel);
        const auto exitLabel = makeLabel();
        Jmp::target(_chunk->inst(jmpNeIdx), exitLabel);

        for(const size_t idx : _breakStack.back()) {
            Jmp::target(_chunk->inst(idx), exitLabel);
        }

        freeRegister(testReg);
    }

    void IRGenerator::generate(const Syntax::BreakStmtNode *node, OptReg &) {
        if(_breakStack.empty()) {
            error("break keyword must in loop or switch scope", node->location);
            return;
        }
        const size_t idx = _chunk->emit<OpCode::Jmp>();
        _breakStack.back().push_back(idx);
    }

    void IRGenerator::generate(const Syntax::ContinueStmtNode *node, OptReg &) {
        if(_continueStack.empty()) {
            error("continue keyword must in loop or switch scope", node->location);
            return;
        }
        if(_continueStack.back().continueLabel.has_value()) {
            Jmp::target(_chunk->inst(_chunk->emit<OpCode::Jmp>()), _continueStack.back().continueLabel.value());
        } else {
            const size_t idx = _chunk->emit<OpCode::Jmp>();
            _continueStack.back().continues.push_back(idx);
        }
    }

    void IRGenerator::generate(const Syntax::TernaryExprNode *node, OptReg &retReg) {
        Register dst = allocateRegister();
        Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<OpCode::Test>(testReg);
        const size_t jmpNeIdx = _chunk->emit<OpCode::JmpNE>();

        Register lhsReg{ 0 };
        if(!expectValue(node->lhsExpr, lhsReg)) {
            return;
        }

        _chunk->emit<OpCode::Mov>(lhsReg, dst);

        const size_t jmpIdx = _chunk->emit<OpCode::Jmp>();
        Jmp::target(_chunk->inst(jmpNeIdx), makeLabel());

        Register rhsReg{ 0 };
        if(!expectValue(node->rhsExpr, rhsReg)) {
            return;
        }

        _chunk->emit<OpCode::Mov>(rhsReg, dst);

        Jmp::target(_chunk->inst(jmpIdx), makeLabel());
        freeRegister(testReg);
        retReg = dst;
    }

    void IRGenerator::generate(const Syntax::ReturnStmtNode *node, OptReg &) {
        if(node->expr) {
            Register reg{ 0 };
            if(!expectValue(node->expr, reg)) {
                return;
            }
            _chunk->emit<OpCode::Ret>(reg);
            return;
        }
        _chunk->emit<OpCode::Ret>(loadVoidReg());
    }

    void IRGenerator::generate(const Syntax::DebuggerStmtNode *, OptReg &) const { _chunk->emit<OpCode::Debugger>(); }

    LocalVariable *IRGenerator::resolveLocalVariable(const Atom identifier) {
        for(auto &var : std::ranges::reverse_view(_localVars)) {
            if(var.endPC.address() == 0 && var.identifier.v == identifier.v) {
                return &var;
            }
        }
        return nullptr;
    }

    FuncMeta *IRGenerator::generateFuncMeta(const Syntax::Parameters &parameters,
                                            const Syntax::BlockStmtNode *body) const {

        auto gen = IRGenerator{ _r, _rt, _sourceFile };
        gen.makeVirtualGlobalScope();

        for(auto &[token, exprNode] : parameters) {
            const auto varName = getAtomFromToken(token);
            OptReg paramReg{};

            if(exprNode) {
                Register defaultParamReg{ 0 };
                if(!gen.expectValue(exprNode, defaultParamReg)) {
                    return nullptr;
                }
                paramReg = defaultParamReg;
            } else {
                paramReg = gen.allocateRegister();
            }
            gen.addLocalVar(LocalVariable{ varName, paramReg.value(), gen.makeLabel() });
        }

        OptReg ignoreReg{};
        auto funChunk = gen.parseAst(body, ignoreReg);
        assert(funChunk);

        // the last instruction is not ret, patch one ret
        if(auto &instVec = funChunk->getInstVec(); instVec.empty() || instVec.back().opcode() != OpCode::Ret) {
            funChunk->emit<OpCode::Ret>(gen.loadVoidReg());
        }

        auto *chunk = _rt.createNoGC<Bytecode::Chunk>(std::move(*funChunk));
        return _rt.createNoGC<FuncMeta>(static_cast<u32>(parameters.size()), chunk, std::move(gen._localVars));
    }

    Register IRGenerator::loadVoidReg() {
        if(!_empty.has_value()) {
            _empty = allocateRegister();
            _chunk->emit<OpCode::Load>(_empty.value(), ConstIdx{ 0 });
        }
        return _empty.value();
    }

    Atom IRGenerator::getAtomFromToken(const Syntax::Token &token) const {
        return _rt.atomTable.intern(token.getString());
    }

    void IRGenerator::genTokenValueLoadInst(Register reg, const Syntax::Token &token) const {
        switch(token.valueType()) {
            case Syntax::TokenValueType::Real:
                _chunk->emit<OpCode::Load>(reg, _chunk->addConstant(token.getReal()));
                break;
            case Syntax::TokenValueType::String:
                _chunk->emit<OpCode::Load>(reg, _chunk->addConstant(getAtomFromToken(token)));
                break;
            case Syntax::TokenValueType::Octet:
                throw std::runtime_error("Unsupported token type octet");
                // return Constant { token.getOctet() };
            case Syntax::TokenValueType::Integer:
                _chunk->emit<OpCode::ILoad>(reg, token.getInteger());
                break;
            case Syntax::TokenValueType::None:
                // loadVoidReg();
                // this is void
                break;
        }
    }

} // namespace cial::Inter
