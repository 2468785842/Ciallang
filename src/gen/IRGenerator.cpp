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

namespace cial::inter {

    Opt<vm::Chunk> IRGenerator::parseAst(const syntax::AstNode *node, OptReg &optReg) {
        if(node)
            node->generateBytecode(this, optReg);
        if(_r.isFailed())
            return {};
        _chunk->setRegCount(_regNextIndex);
        const auto chunk = std::move(_chunk);
        chunk->toBytecode();
        // ===
        // OptimizerManager optimizerManager(*chunk);
        // optimizerManager.addOptimizer(std::make_unique<LoadSubOptimizer>());
        // optimizerManager.applyOptimizations();
        // ===

        _chunk = std::make_unique<vm::Chunk>();
        return vm::Chunk{ std::move(*chunk) };
    }

    bool IRGenerator::expectValue(const syntax::ExprNode *node, Register &ret) {
        OptReg reg{};
        node->generateBytecode(this, reg);

        if(!reg) {
            error("except a return value of expression", node->location);
            return false;
        }

        ret = *reg;
        return true;
    }

    void IRGenerator::generate(const syntax::ExprStmtNode *node, OptReg &optReg) {
        return node->expression->generateBytecode(this, optReg);
    }

    void IRGenerator::generate(const syntax::TryStmtNode *node, OptReg &optReg) {
        const auto tryStartIp = makeLabel(); // closed interval
        node->tryBlock->generateBytecode(this, optReg);

        const size_t jmpIdx = _chunk->emit<NOP>();

        const auto tryEndIp = makeLabel(); // open interval (same as catch start ip closed interval)

        if(node->catchErr) {
            beginScope();

            const Atom varName = getAtomFromToken(node->catchErr->token);
            const Register exValue = allocateRegister();
            addLocalVar(LocalVariable{ varName, exValue, tryEndIp });

            node->catchBlock->generateBytecode(this, optReg);
            _chunk->repl<Jmp>(jmpIdx, makeLabel());

            _chunk->addThrowHandler({ tryStartIp.address(), tryEndIp.address(), exValue.index() });
            endScope();
        } else {
            node->catchBlock->generateBytecode(this, optReg);
            _chunk->repl<Jmp>(jmpIdx, makeLabel());
            _chunk->addThrowHandler({ tryStartIp.address(), tryEndIp.address() });
        }

        optReg = {};
    }

    void IRGenerator::generate(const syntax::ValueExprNode *node, OptReg &optReg) {
        if(node->token.type() == syntax::TokenType::Null) {
            error("null token current not support", node->location);
            return;
        }
        auto dst = allocateRegister();
        genTokenValueLoadInst(dst, node->token);
        optReg = dst;
    }

    void IRGenerator::generate(const syntax::BinaryExprNode *node, OptReg &optReg) {
        using enum syntax::TokenType;

        if(node->token.type() == Dot) {
            Register reg1{ 0 };
            if(!expectValue(node->lhs, reg1))
                return;

            Register dst = allocateRegister();

            if(auto *identifier = dynamic_cast<const syntax::IdentifierExprNode *>(node->rhs); identifier) {
                genTokenValueLoadInst(dst, identifier->token);
                _chunk->emit<GProp>(reg1, dst, dst);
            } else {
                Register reg2{ 0 };
                if(!expectValue(node->rhs, reg2))
                    return;

                _chunk->emit<GProp>(reg1, reg2, dst);
            }
            optReg = dst;
            return;
        }

        if(node->token.type() == Swap) {
            const auto lVarExpr = dynamic_cast<const syntax::IdentifierExprNode *>(node->lhs);
            const auto rVarExpr = dynamic_cast<const syntax::IdentifierExprNode *>(node->rhs);
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
                _chunk->emit<GGlobal>(lVarName, *lVarReg);
            }
            _chunk->emit<Mov>(*lVarReg, dst);
            OptReg rVarReg{};

            if(rVar) {
                rVarReg = rVar->reg;
            } else {
                rVarReg = allocateRegister();
                _chunk->emit<GGlobal>(rVarName, *rVarReg);
            }

            if(lVar) {
                _chunk->emit<Mov>(*rVarReg, *lVarReg);
            } else {
                _chunk->emit<DGlobal>(lVarName, *rVarReg);
            }

            if(rVar) {
                _chunk->emit<Mov>(dst, rVar->reg);
            } else {
                _chunk->emit<DGlobal>(rVarName, dst);
            }
            // optReg = rVarReg;
            return;
        }

        Register src1{ 0 };
        if(!expectValue(node->lhs, src1))
            return;

        Register src2{ 0 };
        if(!expectValue(node->rhs, src2))
            return;

        if(node->token.type() == Comma) {
            optReg = src2;
            return;
        }

        if(node->token.type() == InContextOf) {
            _chunk->emit<ChgThis>(src1, src2);
            optReg = src1;
            return;
        }

        if(node->token.type() == Instanceof) {
            _chunk->emit<ChkIns>(src1, src2);
            optReg = src1;
            return;
        }

        Register dst = allocateRegister();

        switch(node->token.type()) {
            case Equal:
                _chunk->emit<EQ>(src1, src2, dst);
                break;
            case NotEqual:
                _chunk->emit<NEQ>(src1, src2, dst);
                break;
            case DiscEqual:
                _chunk->emit<AbsEQ>(src1, src2, dst);
                break;
            case DiscNotEqual:
                _chunk->emit<AbsNEQ>(src1, src2, dst);
                break;
            case Gt:
                _chunk->emit<GT>(src1, src2, dst);
                break;
            case GtOrEqual:
                _chunk->emit<GE>(src1, src2, dst);
                break;
            case Lt:
                _chunk->emit<LT>(src1, src2, dst);
                break;
            case LtOrEqual:
                _chunk->emit<LE>(src1, src2, dst);
                break;
            case LogicalAnd:
                _chunk->emit<LAnd>(src1, src2, dst);
                break;
            case LogicalOr:
                _chunk->emit<LOr>(src1, src2, dst);
                break;
            case Plus:
                _chunk->emit<Add>(src1, src2, dst);
                break;
            case Minus:
                _chunk->emit<Sub>(src1, src2, dst);
                break;
            case Asterisk:
                _chunk->emit<Mul>(src1, src2, dst);
                break;
            case Slash:
                _chunk->emit<Div>(src1, src2, dst);
                break;
            case Backslash:
                _chunk->emit<Idiv>(src1, src2, dst);
                break;
            case Percent:
                _chunk->emit<Mod>(src1, src2, dst);
                break;
            case Chevron:
                _chunk->emit<BXor>(src1, src2, dst);
                break;
            case VertLine:
                _chunk->emit<BOr>(src1, src2, dst);
                break;
            case Ampersand:
                _chunk->emit<BAnd>(src1, src2, dst);
                break;
            case LArithShift:
                _chunk->emit<BlShift>(src1, src2, dst);
                break;
            case RArithShift:
                _chunk->emit<BrShift>(src1, src2, dst);
                break;
            case RBitShift:
                _chunk->emit<BurShift>(src1, src2, dst);
                break;
            default:
                error("unknow binary operator", node->location);
                return;
        }

        optReg = dst;
    }

    void IRGenerator::generate(const syntax::PrefixUnaryExprNode *node, OptReg &optReg) {
        if(node->token.type() == syntax::TokenType::Throw) {
            Register reg{};
            if(!expectValue(node->rhs, reg)) {
                return;
            }
            _chunk->emit<Throw>(reg);
            // optReg = reg;
            return;
        }

        if(node->token.type() == syntax::TokenType::New) {
            node->rhs->generateBytecode(this, optReg);
            return;
        }

        Register dst = allocateRegister();

        switch(node->token.type()) {
            case syntax::TokenType::Exclamation:
                node->rhs->generateBytecode(this, optReg);
                _chunk->emit<LNot>(*optReg, dst);
                optReg = dst;
                break;
            case syntax::TokenType::Minus:
                node->rhs->generateBytecode(this, optReg);
                _chunk->emit<ChgSign>(*optReg, dst);
                optReg = dst;
                break;
            case syntax::TokenType::Invalidate:
                node->rhs->generateBytecode(this, optReg);
                _chunk->emit<Inv>(*optReg);
                break;
            case syntax::TokenType::Isvalid: {
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<ChkInv>(reg, dst);
                optReg = dst;
                break;
            }
            case syntax::TokenType::Int: {
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<ToInt>(reg, dst);
                optReg = dst;
                break;
            }
            case syntax::TokenType::Real: {
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<ToReal>(reg, dst);
                optReg = dst;
                break;
            }
            case syntax::TokenType::String: {
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<ToString>(reg, dst);
                optReg = dst;
                break;
            }
            default:
                error("unknow prefix unary operator", node->location);
        }
    }


    void IRGenerator::generate(const syntax::SuffixUnaryExprNode *node, OptReg &optReg) {
        // TODO:
        using enum syntax::TokenType;
        switch(node->token.type()) {
            case Isvalid: {
                auto src = allocateRegister();
                if(!expectValue(node->lhs, src)) {
                    return;
                }
                _chunk->emit<ChkInv>(src, *optReg);
                freeRegister(src);
                break;
            }
            case Increment: {
                if(const auto *expr = dynamic_cast<const syntax::IdentifierExprNode *>(node->lhs)) {
                    const Atom identifier = getAtomFromToken(expr->token);

                    if(const auto variable = resolveLocalVariable(identifier)) {
                        Register src2 = variable->reg;
                        auto src1 = allocateRegister();
                        Register dst = allocateRegister();
                        _chunk->emit<LoadImm>(src1, 1);
                        _chunk->emit<Add>(src1, src2, dst);
                        freeRegister(src1);
                        optReg = dst;
                        return;
                    }

                    auto src1 = allocateRegister();
                    auto src2 = allocateRegister();
                    auto dst = allocateRegister();
                    if(isTopScope()) {
                        // global maybe
                        _chunk->emit<LoadImm>(src2, 1);
                        _chunk->emit<GGlobal>(identifier, src1);
                        _chunk->emit<Add>(src2, src1, dst);
                        _chunk->emit<DGlobal>(identifier, dst);
                    } else {
                        // find on context
                        _chunk->emit<LoadImm>(src2, 1);
                        _chunk->emit<GThis>(identifier, src1);
                        _chunk->emit<Add>(src2, src1, dst);
                        _chunk->emit<DThis>(identifier, dst);
                    }
                    freeRegister(src2);
                    optReg = dst;
                    return;
                }
                error("current not support dot chain call", node->location);
                return;
            }
            case Decrement: {

                if(const auto *expr = dynamic_cast<const syntax::IdentifierExprNode *>(node->lhs)) {
                    const Atom identifier = getAtomFromToken(expr->token);

                    if(const auto variable = resolveLocalVariable(identifier)) {
                        Register src2 = variable->reg;
                        auto src1 = allocateRegister();
                        auto dst = allocateRegister();
                        _chunk->emit<LoadImm>(src1, 1);
                        _chunk->emit<Sub>(src1, src2, dst);
                        freeRegister(src1);
                        optReg = dst;
                        return;
                    }

                    auto src1 = allocateRegister();
                    auto src2 = allocateRegister();
                    auto dst = allocateRegister();
                    if(isTopScope()) {
                        // global maybe
                        _chunk->emit<LoadImm>(src2, 1);
                        _chunk->emit<GGlobal>(identifier, src1);
                        _chunk->emit<Sub>(src2, src1, dst);
                        _chunk->emit<DGlobal>(identifier, dst);
                    } else {
                        // find on context
                        _chunk->emit<LoadImm>(src2, 1);
                        _chunk->emit<GThis>(identifier, src1);
                        _chunk->emit<Sub>(src2, src1, dst);
                        _chunk->emit<DThis>(identifier, dst);
                    }
                    freeRegister(src2);
                    optReg = dst;
                    return;
                }
                error("current not support dot chain call", node->location);
                return;
            }
            default:
                error("unknow suffix unary operator", node->location);
        }
    }

    void IRGenerator::generate(const syntax::ProcCallExprNode *node, OptReg &optReg) {
        auto dst = allocateRegister();
        std::vector<Register> arguments{};
        Register memberReg{ 0 };
        if(!expectValue(node->memberAccess, memberReg)) {
            return;
        }

        for(const auto *exprNode : node->arguments) {
            if(!exprNode) {
                _chunk->emit<Push>(loadVoidReg());
            } else {
                Register reg{ 0 };
                if(!expectValue(exprNode, reg)) {
                    return;
                }
                _chunk->emit<Push>(reg);
            }
        }
        freeRegister(memberReg);
        _chunk->emit<Call>(dst, memberReg, static_cast<Integer>(node->arguments.size()));
        // if(!node->arguments.empty()) {
        //     _chunk->emit<PopN>(node->arguments.size());
        // }
        optReg = dst;
    }

    void IRGenerator::generate(const syntax::AssignExprNode *node, OptReg &optReg) {
        if(const auto *expr = dynamic_cast<const syntax::IdentifierExprNode *>(node->lhs)) {
            const Atom identifier = getAtomFromToken(expr->token);

            Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;

            if(const auto variable = resolveLocalVariable(identifier)) {
                Register dst = variable->reg;
                _chunk->emit<Mov>(src, dst);
                freeRegister(src);
                optReg = dst;
                return;
            }

            if(isTopScope()) {
                // global maybe
                _chunk->emit<DGlobal>(identifier, src);
            } else {
                // find on context
                _chunk->emit<DThis>(identifier, src);
            }

            optReg = src;
            return;
        }

        if(const auto *expr = dynamic_cast<const syntax::BinaryExprNode *>(node->lhs)) {

            Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;

            if(expr->token.type() == syntax::TokenType::Dot) {

                Register lhsR{ 0 };
                if(!expectValue(expr->lhs, lhsR))
                    return;

                if(const auto *identifierExpr = dynamic_cast<const syntax::IdentifierExprNode *>(expr->rhs)) {
                    auto tmpR = allocateRegister();
                    genTokenValueLoadInst(tmpR, identifierExpr->token);
                    _chunk->emit<DProp>(lhsR, tmpR, src);
                    freeRegister(tmpR);

                    optReg = src;
                    return;
                }
            }
        }

        error("isn't support assign operator", node->location);
    }

    void IRGenerator::generate(const syntax::FunctionExprNode *node, OptReg &optReg) {
        auto funReg = allocateRegister();
        auto *funcMeta = generateFuncMeta(node->parameters, node->body);

        if(!funcMeta) {
            error("generate chunk failed with function expr", node->location);
            return;
        }

        _chunk->emit<Load>(funReg, _chunk->addConstant(funcMeta));

        optReg = funReg;
    }

    void IRGenerator::generate(const syntax::PropertyDeclNode *node, OptReg &) {
        FuncMeta *setFuncMeta{};
        if(node->setter)
            setFuncMeta = generateFuncMeta(node->setter->parameters, node->setter->body);

        FuncMeta *getFuncMeta;
        if(node->getter)
            getFuncMeta = generateFuncMeta(node->getter->parameters, node->getter->body);

        auto *propMeta = _rt.createNoGC<PropMeta>(setFuncMeta, getFuncMeta);

        const auto identifier = getAtomFromToken(node->token);

        auto propReg = allocateRegister();
        _chunk->emit<Load>(propReg, _chunk->addConstant(propMeta));

        if(isTopScope()) {
            freeRegister(propReg);
            _chunk->emit<DGlobal>(identifier, propReg);
            return;
        }

        addLocalVar(LocalVariable{ identifier, propReg, makeLabel() });
    }

    void IRGenerator::generate(const syntax::VarDeclNode *node, OptReg &) {
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
                _chunk->emit<DGlobal>(identifier, src);
                return;
            }

            // already have this variable, in same scope
            if(const auto variable = resolveLocalVariable(identifier)) {
                freeRegister(src);
                _chunk->emit<Mov>(src, variable->reg);
                return;
            }

            // not found but can init
            addLocalVar(LocalVariable{ identifier, src, makeLabel() });
            return;
        }

        // global
        if(isTopScope()) {
            _chunk->emit<DGlobal>(identifier, loadVoidReg());
            return;
        }

        // not found and can't init
        addLocalVar(LocalVariable{ identifier, loadVoidReg(), makeLabel() });
    }

    void IRGenerator::generate(const syntax::FunctionDeclNode *node, OptReg &) {
        auto funReg = allocateRegister();
        auto *funcMeta = generateFuncMeta(node->parameters, node->body);

        if(!funcMeta) {
            error("generate chunk failed with function declaration", node->location);
            return;
        }

        const auto identifier = getAtomFromToken(node->token);
        funcMeta->name = identifier;
        _chunk->emit<Load>(funReg, _chunk->addConstant(funcMeta));

        if(isTopScope()) {
            freeRegister(funReg);
            _chunk->emit<DGlobal>(identifier, funReg);
            return;
        }

        addLocalVar(LocalVariable{ identifier, funReg, makeLabel() });
    }


    void IRGenerator::generate(const syntax::ClassDeclNode *node, OptReg &) {
        const auto identifier = getAtomFromToken(node->token);

        auto classReg = allocateRegister();
        auto *classMeta = _rt.createNoGC<ClassMeta>(identifier, 0);
        _chunk->emit<Load>(classReg, _chunk->addConstant(classMeta));

        // class is always global in current design
        _chunk->emit<DGlobal>(identifier, classReg);

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
                    gen._chunk->emit<DThis>(varName, defaultVarReg);
                }

                classMeta->setMember(MemberShapeMeta{ .name = varName, .isVar = true }, ClassFieldMeta{});
            }
            OptReg ignoreReg{};
            auto funChunk = gen.parseAst(nullptr, ignoreReg);
            assert(funChunk);

            auto *chunk = _rt.createNoGC<vm::Chunk>(std::move(*funChunk));
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
                funChunk->emit<Ret>(voidReg);

                auto *chunk = _rt.createNoGC<vm::Chunk>(std::move(*funChunk));
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
            auto funChunk = _rt.createNoGC<vm::Chunk>(*genFinalize.parseAst(nullptr, ignoreReg));

            auto voidReg = genFinalize.loadVoidReg();
            funChunk->emit<Ret>(voidReg);

            classMeta->setMember(MemberShapeMeta{ .name = finalizeAtom, .isMethod = true },
                                 ClassFieldMeta{ _rt.createNoGC<FuncMeta>(0, funChunk, Vec<LocalVariable>{}) });
        }
        endScope();
        freeRegister(classReg);
    }

    void IRGenerator::generate(const syntax::IdentifierExprNode *node, OptReg &optReg) {
        auto dst = allocateRegister();
        const auto identifier = getAtomFromToken(node->token);

        if(const auto variable = resolveLocalVariable(identifier)) {
            _chunk->emit<CP>(variable->reg, dst);
            optReg = dst;
            return;
        }

        if(isTopScope()) {
            _chunk->emit<GGlobal>(identifier, dst);
        } else {
            // dynamic get
            // but tjs2 doesn't support get up function scope local var
            // _chunk->emit<GUpval>(identifier, dst);
            // find on context
            _chunk->emit<GThis>(identifier, dst);
        }

        optReg = dst;
    }

    void IRGenerator::generate(const syntax::InternalIdentifierExprNode *node, OptReg &optReg) {
        auto dst = allocateRegister();

        switch(node->token.type()) {
            case syntax::TokenType::Global:
                _chunk->emit<Global>(dst);
                break;
            case syntax::TokenType::Super:
                _chunk->emit<Super>(dst);
                break;
            case syntax::TokenType::This:
                _chunk->emit<This>(dst);
                break;
            default:
                error("not support", node->location);
                return;
        }

        optReg = dst;
    }

    void IRGenerator::generate(const syntax::StmtDeclNode *node, OptReg &optReg) {
        return node->statement->generateBytecode(this, optReg);
    }

    void IRGenerator::generate(const syntax::BlockStmtNode *node, OptReg &) {
        // block statement never return value
        beginScope();
        OptReg ignore{};
        for(const auto children : node->childrens) {
            children->generateBytecode(this, ignore);
        }
        endScope();
    }

    void IRGenerator::generate(const syntax::IfStmtNode *node, OptReg &) {
        OptReg ignore{};
        Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<Test>(testReg);

        const size_t jmpNeIdx = _chunk->emit<NOP>();
        node->body->generateBytecode(this, ignore);
        size_t jmpIdx{};

        if(node->elseBody) {
            jmpIdx = _chunk->emit<NOP>();
        }

        _chunk->repl<JmpNE>(jmpNeIdx, makeLabel());

        if(node->elseBody) {
            node->elseBody->generateBytecode(this, ignore);
            _chunk->repl<JmpNE>(jmpIdx, makeLabel());
        }

        freeRegister(testReg);
    }

    void IRGenerator::generate(const syntax::SwitchStmtNode *node, OptReg &) {
        using namespace vm;

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

                Register dst = allocateRegister();
                _chunk->emit<EQ>(testReg, matchReg, dst);
                _chunk->emit<Test>(dst);
                freeRegister(dst);
                jmpEIdxVec.push_back(_chunk->emit<NOP>());
            }

            const size_t jmpIdx = _chunk->emit<NOP>();
            for(const size_t jmpEIdx : jmpEIdxVec) {
                _chunk->repl<JmpE>(jmpEIdx, makeLabel());
            }

            if(i != 0)
                _chunk->repl<Jmp>(prevCaseJmpIdx, makeLabel());

            body->generateBytecode(this, ignore);

            if(body != node->matches.back().second)
                prevCaseJmpIdx = _chunk->emit<NOP>();

            _chunk->repl<Jmp>(jmpIdx, makeLabel());
        }

        if(node->defaultBody)
            node->defaultBody->generateBytecode(this, ignore);

        const auto exitLabel = makeLabel();

        for(const size_t br : _breakStack.back()) {
            _chunk->repl<Jmp>(br, exitLabel);
        }
    }

    void IRGenerator::generate(const syntax::DoWhileStmtNode *node, OptReg &) {
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
            _chunk->repl<Jmp>(idx, testLabel);
        }

        Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<Test>(testReg);
        const size_t jmpNEIdx = _chunk->emit<NOP>();
        _chunk->emit<Jmp>(bodyLabel);

        const auto exitLabel = makeLabel();
        _chunk->repl<JmpNE>(jmpNEIdx, exitLabel);
        for(const size_t idx : _breakStack.back()) {
            _chunk->repl<Jmp>(idx, exitLabel);
        }

        freeRegister(testReg);
    }

    void IRGenerator::generate(const syntax::ForStmtNode *node, OptReg &) {
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
            _chunk->emit<Test>(testReg);
            jmpNeIdx = _chunk->emit<NOP>();
        }

        node->body->generateBytecode(this, ignore);

        const auto stepLabel = makeLabel();
        _continueStack.back().continueLabel = stepLabel;
        for(const size_t idx : _continueStack.back().continues) {
            _chunk->repl<Jmp>(idx, stepLabel);
        }

        if(node->step) {
            Register stepReg{ 0 };
            if(!expectValue(node->step, stepReg)) {
                return;
            }
            freeRegister(stepReg);
        }

        _chunk->emit<Jmp>(testLabel);

        const auto exitLabel = makeLabel();
        if(node->test)
            _chunk->repl<JmpNE>(jmpNeIdx, exitLabel);
        for(const size_t idx : _breakStack.back()) {
            _chunk->repl<Jmp>(idx, exitLabel);
        }

        if(node->test)
            freeRegister(testReg);
    }

    void IRGenerator::generate(const syntax::WhileStmtNode *node, OptReg &) {
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

        _chunk->emit<Test>(testReg);
        const size_t jmpNeIdx = _chunk->emit<NOP>();

        node->body->generateBytecode(this, ignore);

        const auto testLabel = makeLabel();
        _continueStack.back().continueLabel = testLabel;
        for(const size_t idx : _continueStack.back().continues) {
            _chunk->repl<Jmp>(idx, testLabel);
        }

        _chunk->emit<Jmp>(loopLabel);
        const auto exitLabel = makeLabel();
        _chunk->repl<JmpNE>(jmpNeIdx, exitLabel);

        for(const size_t idx : _breakStack.back()) {
            _chunk->repl<Jmp>(idx, exitLabel);
        }

        freeRegister(testReg);
    }

    void IRGenerator::generate(const syntax::BreakStmtNode *node, OptReg &) {
        if(_breakStack.empty()) {
            error("break keyword must in loop or switch scope", node->location);
            return;
        }
        const size_t idx = _chunk->emit<NOP>();
        _breakStack.back().push_back(idx);
    }

    void IRGenerator::generate(const syntax::ContinueStmtNode *node, OptReg &) {
        if(_continueStack.empty()) {
            error("continue keyword must in loop or switch scope", node->location);
            return;
        }
        if(_continueStack.back().continueLabel.has_value()) {
            _chunk->emit<Jmp>(_continueStack.back().continueLabel.value());
        } else {
            const size_t idx = _chunk->emit<NOP>();
            _continueStack.back().continues.push_back(idx);
        }
    }

    void IRGenerator::generate(const syntax::TernaryExprNode *node, OptReg &optReg) {
        Register dst = allocateRegister();
        Register testReg{ 0 };
        if(!expectValue(node->test, testReg)) {
            return;
        }

        _chunk->emit<Test>(testReg);
        const size_t jmpNeIdx = _chunk->emit<NOP>();

        Register lhsReg{ 0 };
        if(!expectValue(node->lhsExpr, lhsReg)) {
            return;
        }

        _chunk->emit<Mov>(lhsReg, dst);

        const size_t jmpIdx = _chunk->emit<NOP>();
        _chunk->repl<JmpNE>(jmpNeIdx, makeLabel());

        Register rhsReg{ 0 };
        if(!expectValue(node->rhsExpr, rhsReg)) {
            return;
        }

        _chunk->emit<Mov>(rhsReg, dst);

        _chunk->repl<Jmp>(jmpIdx, makeLabel());
        freeRegister(testReg);
        optReg = dst;
    }

    void IRGenerator::generate(const syntax::ReturnStmtNode *node, OptReg &) {
        if(node->expr) {
            Register reg{ 0 };
            if(!expectValue(node->expr, reg)) {
                return;
            }
            _chunk->emit<Ret>(reg);
            return;
        }
        _chunk->emit<Ret>(loadVoidReg());
    }

    void IRGenerator::generate(const syntax::DebuggerStmtNode *, OptReg &) { _chunk->emit<Debugger>(); }

    LocalVariable *IRGenerator::resolveLocalVariable(const Atom identifier) {
        for(auto &var : std::ranges::reverse_view(_localVars)) {
            if(var.endPC.address() == 0 && var.identifier.v == identifier.v) {
                return &var;
            }
        }
        return nullptr;
    }

    FuncMeta *IRGenerator::generateFuncMeta(const syntax::Parameters &parameters,
                                            const syntax::BlockStmtNode *body) const {

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
        if(auto &instVec = funChunk->getInstVec(); instVec.empty() || instVec.back()->opcode() != TacOpCode::Ret) {
            funChunk->emit<Ret>(gen.loadVoidReg());
        }

        auto *chunk = _rt.createNoGC<vm::Chunk>(std::move(*funChunk));
        return _rt.createNoGC<FuncMeta>(static_cast<u32>(parameters.size()), chunk, std::move(gen._localVars));
    }

    Register IRGenerator::loadVoidReg() {
        if(!_empty.has_value()) {
            _empty = allocateRegister();
            _chunk->emit<Load>(_empty.value(), ConstIdx{ 0 });
        }
        return _empty.value();
    }

    Atom IRGenerator::getAtomFromToken(const syntax::Token &token) const {
        return _rt.atomTable.intern(token.getString());
    }

    void IRGenerator::genTokenValueLoadInst(Register reg, const syntax::Token &token) const {
        switch(token.valueType()) {
            case syntax::TokenValueType::Real:
                _chunk->emit<Load>(reg, _chunk->addConstant(token.getReal()));
                break;
            case syntax::TokenValueType::String:
                _chunk->emit<Load>(reg, _chunk->addConstant(getAtomFromToken(token)));
                break;
            case syntax::TokenValueType::Octet:
                throw std::runtime_error("Unsupported token type octet");
                // return Constant { token.getOctet() };
            case syntax::TokenValueType::Integer:
                _chunk->emit<LoadImm>(reg, token.getInteger());
                break;
            case syntax::TokenValueType::None:
                // loadVoidReg();
                // this is void
                break;
        }
    }

} // namespace cial::inter
