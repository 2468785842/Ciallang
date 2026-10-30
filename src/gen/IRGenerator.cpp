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

#include <fmt/format.h>

#include "Optimizer.hpp"
#include "common/Defer.hpp"
#include "runtime/Runtime.hpp"

#include "parser/ast/DeclNode.hpp"
#include "parser/ast/ExprNode.hpp"
#include "parser/ast/StmtNode.hpp"

#include "Instruction.hpp"

namespace cial::inter {

    IRGenerator::IRGenerator(Common::Result &r, Runtime &rt, Common::SourceFile &sourceFile) :
        _rt(rt), _sourceFile(sourceFile), _r(r) {}

    IRGenerator::~IRGenerator() = default;

    Opt<vm::Chunk> IRGenerator::parseAst(const syntax::AstNode *node, OptReg &optReg) {
        _rt.beginCompilation();
        DEFER { _rt.endCompilation(); };
        if(node)
            node->generateBytecode(this, optReg);
        if(_r.isFailed())
            return {};
        _chunk->setRegCount(_regNextIndex);
        const auto chunk = std::move(_chunk);
        // 字节码由 VM 进入帧时按需生成。
        OptimizerManager optimizerManager(*chunk);
        optimizerManager.addOptimizer(std::make_unique<CopyPropagationOptimizer>());
        optimizerManager.addOptimizer(std::make_unique<IntegerConstantPropagationOptimizer>());
        optimizerManager.addOptimizer(std::make_unique<DeadLoadEliminationOptimizer>(optReg));
        optimizerManager.addOptimizer(std::make_unique<CompareTestFusionOptimizer>());
        const OptimizerStats optimizerStats = optimizerManager.applyOptimizations();
        if(OptimizerManager::profilingEnabled()) {
            fmt::print(stderr,
                       "[optimizer] TAC {} -> {}, removed={} by compaction, CFG {:.3f} us, compaction {:.3f} us",
                       optimizerStats.instructionsBefore, optimizerStats.instructionsAfter,
                       optimizerStats.instructionsRemovedByCompaction,
                       static_cast<double>(optimizerStats.cfgNanoseconds) / 1000.0,
                       static_cast<double>(optimizerStats.compactionNanoseconds) / 1000.0);
            for(const auto &pass : optimizerStats.passes)
                fmt::print(stderr, ", {} {:.3f} us (TAC {} -> {}, transforms={})", pass.name,
                           static_cast<double>(pass.nanoseconds) / 1000.0, pass.instructionsBefore,
                           pass.instructionsAfter, pass.transformations);
            fmt::print(stderr, "\n");
        }

        _chunk = std::make_unique<vm::Chunk>();
        return vm::Chunk{ std::move(*chunk) };
    }

    bool IRGenerator::expectValue(const syntax::ExprNode *node, Register &ret) {
        // 哨兵寄存器（0xFFFF 不会被正常分配）：告知表达式生成器"调用方需要值"，
        // 后缀 ++/-- 据此决定是否发射旧值拷贝；语句链传入的空 OptReg 则不拷贝
        OptReg reg{ Register{ 0xFFFF } };
        node->generateBytecode(this, reg);

        if(!reg || reg->index() == 0xFFFF) {
            error("except a return value of expression", node->location);
            return false;
        }

        ret = *reg;
        return true;
    }

    void IRGenerator::generate(const syntax::ExprStmtNode *node, OptReg &optReg) {
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
        return node->expression->generateBytecode(this, optReg);
    }

    void IRGenerator::generate(const syntax::TryStmtNode *node, OptReg &optReg) {
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
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

        // tjs.y:661 `A in B`：原版保留字，tjsInterCodeGen 无 T_IN 分支，整个节点
        // （含两侧操作数）不求值，结果寄存器为 void。行为对齐锁定，勿改成成员存在判断
        if(node->token.type() == In) {
            optReg = loadVoidReg();
            return;
        }

        if(node->token.type() == Dot) {
            Register reg1{ 0 };
            if(!expectValue(node->lhs, reg1))
                return;

            Register dst = allocateRegister();

            if(auto *identifier = dynamic_cast<const syntax::IdentifierExprNode *>(node->rhs); identifier) {
                // 成员名编译期 Atom 直引（跳过名字字符串寄存器与 dispatch 再 intern）
                _chunk->emit<GPropC>(reg1, getAtomFromToken(identifier->token), dst);
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
            _chunk->emit<DLocal>(*lVarReg, dst);
            OptReg rVarReg{};

            if(rVar) {
                rVarReg = rVar->reg;
            } else {
                rVarReg = allocateRegister();
                _chunk->emit<GGlobal>(rVarName, *rVarReg);
            }

            if(lVar) {
                _chunk->emit<DLocal>(*rVarReg, *lVarReg);
            } else {
                _chunk->emit<DGlobal>(lVarName, *rVarReg);
            }

            if(rVar) {
                _chunk->emit<DLocal>(dst, rVar->reg);
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
            case syntax::TokenType::Plus: {
                // 前缀 +:TJS 语义为"确保数值"——字符串转数值(+str5 == 5),
                // octet 无法转数值则报错(语料 test_variant 期望 +oct5 被 catch)。
                // 降糖为 ToReal(宽松比较下整数/实数等价)
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<ToReal>(reg, dst);
                freeRegister(reg);
                optReg = dst;
                break;
            }
            case syntax::TokenType::Tilde: {
                // ~x 降糖为 x ^ -1(整数按位取反),零新指令
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                Register neg1 = allocateRegister();
                _chunk->emit<LoadImm>(neg1, -1);
                _chunk->emit<BXor>(reg, neg1, dst);
                freeRegister(neg1);
                optReg = dst;
                break;
            }
            case syntax::TokenType::Typeof: {
                // typeof expr -> 类型名字符串(运行期按 Value 类型分派,与 TJS 语义一致)
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<TypeOf>(reg, dst);
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
            case syntax::TokenType::Increment:
            case syntax::TokenType::Decrement: {
                // 前缀 ++/--：表达式值为自增/自减后的新值
                const auto *identifier = dynamic_cast<const syntax::IdentifierExprNode *>(node->rhs);
                if(!identifier) {
                    error("prefix ++/-- expects identifier operand", node->location);
                    return;
                }
                generateIncDec(getAtomFromToken(identifier->token), node->token.type() == syntax::TokenType::Increment,
                               optReg);
                break;
            }
            case syntax::TokenType::Sharp: {
                // #s：字符串首字符的字符码（旧版 VM_ASC）
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<Asc>(reg, dst);
                freeRegister(reg);
                optReg = dst;
                break;
            }
            case syntax::TokenType::Dollar: {
                // $n：字符码转 1 字符字符串（旧版 VM_CHR）
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<Chr>(reg, dst);
                freeRegister(reg);
                optReg = dst;
                break;
            }
            case syntax::TokenType::Delete: {
                // delete obj.member：仅支持成员访问形式（TJS2 delete 作用于对象成员）
                const auto *member = dynamic_cast<const syntax::BinaryExprNode *>(node->rhs);
                if(!member || member->token.type() != syntax::TokenType::Dot) {
                    error("delete expects member access (obj.member)", node->location);
                    return;
                }
                Register objReg{};
                if(!expectValue(member->lhs, objReg)) {
                    return;
                }
                const auto *identifierExpr = dynamic_cast<const syntax::IdentifierExprNode *>(member->rhs);
                if(!identifierExpr) {
                    error("delete member must be identifier", node->location);
                    return;
                }
                // 成员名编译期 Atom 直引
                _chunk->emit<DelPropC>(objReg, getAtomFromToken(identifierExpr->token), dst);
                optReg = dst;
                break;
            }
            case syntax::TokenType::Ampersand: {
                // &obj.member / &identifier（旧版 T_IGNOREPROP/GPDS）：取成员本体，
                // property 不触发 getter。
                // identifier 形态：局部变量直接拷值；否则 GRefRaw 走 this/global 槽位
                const syntax::Token *memberToken{};
                const syntax::ExprNode *objExpr{};
                if(const auto *identifier = dynamic_cast<const syntax::IdentifierExprNode *>(node->rhs)) {
                    const Atom identifierAtom = getAtomFromToken(identifier->token);
                    if(const auto variable = resolveLocalVariable(identifierAtom)) {
                        // 局部变量无 property 包装，本体即值
                        const Register dst2 = allocateRegister();
                        _chunk->emit<DLocal>(variable->reg, dst2);
                        optReg = dst2;
                        break;
                    }
                    _chunk->emit<GRefRaw>(identifierAtom, dst);
                    optReg = dst;
                    break;
                }
                if(const auto *member = dynamic_cast<const syntax::BinaryExprNode *>(node->rhs);
                   member && member->token.type() == syntax::TokenType::Dot) {
                    objExpr = member->lhs;
                    if(const auto *identifierExpr = dynamic_cast<const syntax::IdentifierExprNode *>(member->rhs)) {
                        memberToken = &identifierExpr->token;
                    } else {
                        error("'&' member must be identifier", node->location);
                        return;
                    }
                } else {
                    error("'&' expects identifier or member access", node->location);
                    return;
                }

                Register objReg{};
                if(!expectValue(objExpr, objReg)) {
                    return;
                }

                _chunk->emit<GPropRawC>(objReg, getAtomFromToken(*memberToken), dst);
                freeRegister(objReg);
                optReg = dst;
                break;
            }
            case syntax::TokenType::Asterisk: {
                // *p（旧版 T_PROPACCESS/GETP）：强制属性访问读，调 getter
                Register reg{};
                if(!expectValue(node->rhs, reg)) {
                    return;
                }
                _chunk->emit<DerefGet>(reg, dst);
                freeRegister(reg);
                optReg = dst;
                break;
            }
            default:
                error("unknow prefix unary operator", node->location);
        }
    }


    void IRGenerator::generate(const syntax::SuffixUnaryExprNode *node, OptReg &optReg) {
        using enum syntax::TokenType;
        switch(node->token.type()) {
            case Exclamation: {
                // 后缀 `!` 即时求值（tjs.y priority_expr "!" → T_EVAL；原版 Eval）：
                // A 字符串化为 TJS 表达式编译执行；空串/非串原值不变
                Register src{};
                if(!expectValue(node->lhs, src)) {
                    return;
                }
                const Register dst = allocateRegister();
                _chunk->emit<EvalStr>(src, dst);
                optReg = dst;
                freeRegister(src);
                break;
            }
            case Isvalid: {
                Register src{};
                if(!expectValue(node->lhs, src)) {
                    return;
                }
                const auto dst = allocateRegister();
                _chunk->emit<ChkInv>(src, dst);
                optReg = dst;
                freeRegister(src);
                break;
            }
            case Increment:
            case Decrement: {
                if(const auto *expr = dynamic_cast<const syntax::IdentifierExprNode *>(node->lhs)) {
                    generateIncDec(getAtomFromToken(expr->token), node->token.type() == Increment, optReg,
                                   /*postfix=*/true);
                    return;
                }
                if(const auto *member = dynamic_cast<const syntax::BinaryExprNode *>(node->lhs);
                   member && member->token.type() == Dot) {
                    // obj.member++/--（旧版 INCGETP/SUBGETP 语义）：getter 读旧值 →
                    // 加减 → setter 写新值；表达式值为旧值（仅调用方需要时拷贝）
                    const auto *identifierExpr = dynamic_cast<const syntax::IdentifierExprNode *>(member->rhs);
                    if(!identifierExpr) {
                        error("suffix ++/-- member must be identifier", node->location);
                        return;
                    }
                    Register objReg{ 0 };
                    if(!expectValue(member->lhs, objReg))
                        return;
                    const Atom memberAtom = getAtomFromToken(identifierExpr->token);

                    Register curReg = allocateRegister();
                    _chunk->emit<GPropC>(objReg, memberAtom, curReg); // 触发 getter
                    Register oldReg{};
                    if(optReg) {
                        oldReg = allocateRegister();
                        _chunk->emit<DLocal>(curReg, oldReg);
                    }
                    const Register one = allocateRegister();
                    _chunk->emit<LoadImm>(one, 1);
                    const Register nv = allocateRegister();
                    if(node->token.type() == Increment) {
                        _chunk->emit<Add>(one, curReg, nv);
                    } else {
                        // 注意操作数顺序：变量 - 1
                        _chunk->emit<Sub>(curReg, one, nv);
                    }
                    _chunk->emit<DPropC>(objReg, memberAtom, nv); // 触发 setter（压帧 → Call）

                    freeRegister(one);
                    freeRegister(nv);
                    freeRegister(curReg);
                    freeRegister(objReg);
                    if(optReg)
                        optReg = oldReg;
                    return;
                }
                error("current not support dot chain call", node->location);
                return;
            }
            default:
                error("unknow suffix unary operator", node->location);
        }
    }

    void IRGenerator::generateIncDec(const Atom identifier, const bool isIncrement, OptReg &optReg,
                                     const bool postfix) {
        // 后缀旧值拷贝仅在调用方需要表达式值时发射（语句上下文 optReg 为空，
        // i++; 不付拷贝成本）
        const bool needOld = postfix && static_cast<bool>(optReg);
        if(const auto variable = resolveLocalVariable(identifier)) {
            Register src2 = variable->reg;
            auto src1 = allocateRegister();
            Register dst = allocateRegister();
            Register oldReg{};
            if(needOld) {
                // TJS2 后缀语义：表达式值为自增/自减前的旧值
                oldReg = allocateRegister();
                _chunk->emit<DLocal>(src2, oldReg);
            }
            _chunk->emit<LoadImm>(src1, 1);
            if(isIncrement) {
                _chunk->emit<Add>(src1, src2, dst);
            } else {
                // 注意操作数顺序：变量 - 1，而非 1 - 变量
                _chunk->emit<Sub>(src2, src1, dst);
            }
            // 局部变量必须写回，否则 ++/-- 永远不生效
            _chunk->emit<DLocal>(dst, src2);
            freeRegister(src1);
            freeRegister(dst);
            // 前缀值=新值 dst；后缀仅在需要时给出旧值
            if(needOld)
                optReg = oldReg;
            else if(postfix)
                optReg = {};
            else
                optReg = dst;
            return;
        }

        auto src1 = allocateRegister();
        auto src2 = allocateRegister();
        auto dst = allocateRegister();
        Register oldReg{};
        if(needOld) {
            oldReg = allocateRegister();
        }
        _chunk->emit<LoadImm>(src2, 1);
        if(isTopScope()) {
            // global maybe
            _chunk->emit<GGlobal>(identifier, src1);
            if(needOld) {
                _chunk->emit<DLocal>(src1, oldReg);
            }
            if(isIncrement) {
                _chunk->emit<Add>(src2, src1, dst);
            } else {
                // 注意操作数顺序：变量 - 1，而非 1 - 变量
                _chunk->emit<Sub>(src1, src2, dst);
            }
            _chunk->emit<DGlobal>(identifier, dst);
        } else {
            // find on context
            _chunk->emit<GThis>(identifier, src1);
            if(needOld) {
                _chunk->emit<DLocal>(src1, oldReg);
            }
            if(isIncrement) {
                _chunk->emit<Add>(src2, src1, dst);
            } else {
                // 注意操作数顺序：变量 - 1，而非 1 - 变量
                _chunk->emit<Sub>(src1, src2, dst);
            }
            _chunk->emit<DThis>(identifier, dst);
        }
        freeRegister(src2);
        freeRegister(dst);
        if(needOld)
            optReg = oldReg;
        else if(postfix)
            optReg = {};
        else
            optReg = dst;
    }

    void IRGenerator::generate(const syntax::ProcCallExprNode *node, OptReg &optReg) {
        auto dst = allocateRegister();

        // `...`：省略透传（旧版 argc=-1）——转发本帧实际实参
        const bool omitCall = std::any_of(node->arguments.begin(), node->arguments.end(), [](const auto *expr) {
            return dynamic_cast<const syntax::OmitArgExprNode *>(expr) != nullptr;
        });

        // `expr*` / 裸 `*`：展开调用（旧版 argc=-2 条目表）
        bool expandCall = false;
        if(!omitCall) {
            for(const auto *exprNode : node->arguments) {
                if(exprNode && dynamic_cast<const syntax::ExpandArgExprNode *>(exprNode)) {
                    expandCall = true;
                    break;
                }
            }
        }

        std::vector<Register> arguments{};
        Register memberReg{ 0 };
        if(!expectValue(node->memberAccess, memberReg)) {
            return;
        }

        Opt<Register> receiver;
        if(!omitCall && !expandCall && !_chunk->getInstVec().empty() &&
           _chunk->getInstVec().back()->opcode() == TacOpCode::GPropC) {
            const auto &get = *_chunk->getInstVec().back();
            if(get.getOp3() == memberReg) {
                const auto obj = get.getOp1().value<Register>();
                const auto atom = get.getOp2().value<Atom>();
                receiver = allocateRegister();
                _chunk->getInstVec().back() = std::make_unique<GMethodC>(obj, atom, memberReg, *receiver);
            }
        }

        if(omitCall) {
            freeRegister(memberReg);
            _chunk->emitRaw<CallOmit>(dst, memberReg);
            optReg = dst;
            return;
        }

        if(expandCall) {
            using FatEntry = std::pair<u8, Register>;
            Vec<FatEntry> entries{};
            for(const auto *exprNode : node->arguments) {
                if(!exprNode) {
                    // 缺位实参：void 常量寄存器（Normal）
                    entries.emplace_back(0, loadVoidReg());
                    continue;
                }
                if(const auto *expand = dynamic_cast<const syntax::ExpandArgExprNode *>(exprNode)) {
                    if(expand->inner) {
                        Register reg{ 0 };
                        if(!expectValue(expand->inner, reg)) {
                            return;
                        }
                        entries.emplace_back(1, reg); // fatExpand：展开 Array 元素
                    } else {
                        entries.emplace_back(2, Register{ 0 }); // fatUnnamedExpand
                    }
                    continue;
                }
                Register reg{ 0 };
                if(!expectValue(exprNode, reg)) {
                    return;
                }
                entries.emplace_back(0, reg); // fatNormal
            }
            freeRegister(memberReg);
            _chunk->emitRaw<CallExpand>(dst, memberReg, Integer{ unnamedArgBase }, std::move(entries));
            optReg = dst;
            return;
        }

        std::vector<Register> argRegs{};
        for(const auto *exprNode : node->arguments) {
            if(!exprNode) {
                argRegs.push_back(loadVoidReg()); // 缺位实参：void 常量寄存器
            } else {
                Register reg{ 0 };
                if(!expectValue(exprNode, reg)) {
                    return;
                }
                argRegs.push_back(reg);
            }
        }
        freeRegister(memberReg);
        // 变长实参单指令调用（消逐参 Push 派发；语义等价）
        if(receiver) {
            _chunk->emitRaw<CallMethodV>(dst, memberReg, static_cast<Integer>(node->arguments.size()),
                                         std::move(argRegs), *receiver);
            freeRegister(*receiver);
        } else {
            _chunk->emitRaw<CallV>(dst, memberReg, static_cast<Integer>(node->arguments.size()), std::move(argRegs));
        }
        optReg = dst;
    }

    // `...` / `*` 只在调用实参表内合法（ProcCall 生成时已特判降为
    // CallOmit/CallExpand）；单独到达此处即语法位置错误
    void IRGenerator::generate(const syntax::OmitArgExprNode *node, OptReg &optReg) {
        error("`...` is only allowed in a call argument list", node->location);
    }

    void IRGenerator::generate(const syntax::ExpandArgExprNode *node, OptReg &optReg) {
        error("`*` expand is only allowed in a call argument list", node->location);
    }

    void IRGenerator::generate(const syntax::AssignExprNode *node, OptReg &optReg) {
        if(const auto *expr = dynamic_cast<const syntax::IdentifierExprNode *>(node->lhs)) {
            const Atom identifier = getAtomFromToken(expr->token);

            Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;

            if(const auto variable = resolveLocalVariable(identifier)) {
                Register dst = variable->reg;
                _chunk->emit<DLocal>(src, dst);
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
                    // 成员名编译期 Atom 直引
                    _chunk->emit<DPropC>(lhsR, getAtomFromToken(identifierExpr->token), src);

                    optReg = src;
                    return;
                }
            }
        }

        if(const auto *expr = dynamic_cast<const syntax::PrefixUnaryExprNode *>(node->lhs);
           expr && expr->token.type() == syntax::TokenType::Asterisk) {
            // *p = v（旧版 SETP）：通过 property 引用写，调 setter
            Register propReg{ 0 };
            if(!expectValue(expr->rhs, propReg))
                return;

            Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;

            _chunk->emit<DerefSet>(propReg, src);
            freeRegister(propReg);
            freeRegister(src);
            optReg = src;
            return;
        }

        if(const auto *expr = dynamic_cast<const syntax::PrefixUnaryExprNode *>(node->lhs);
           expr && expr->token.type() == syntax::TokenType::Ampersand) {
            // &obj.member = v（旧版 SPDS）：绕过 setter 直接写槽位（注册 property 引用）
            const auto *member = dynamic_cast<const syntax::BinaryExprNode *>(expr->rhs);
            if(!member || member->token.type() != syntax::TokenType::Dot ||
               !dynamic_cast<const syntax::IdentifierExprNode *>(member->rhs)) {
                error("'&' assignment expects member access lhs", node->location);
                return;
            }
            Register objReg{ 0 };
            if(!expectValue(member->lhs, objReg))
                return;
            Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;
            _chunk->emit<DPropRawC>(
                objReg, getAtomFromToken(dynamic_cast<const syntax::IdentifierExprNode *>(member->rhs)->token), src);
            freeRegister(objReg);
            freeRegister(src);
            optReg = src;
            return;
        }

        if(const auto *expr = dynamic_cast<const syntax::IndexExprNode *>(node->lhs)) {
            // a[i] = v：下标写（数组越界自动扩展/字典键写入，SetIndex 分派）
            Register objReg{ 0 };
            if(!expectValue(expr->lhs, objReg))
                return;
            Register idxReg{ 0 };
            if(!expectValue(expr->index, idxReg))
                return;
            Register src{ 0 };
            if(!expectValue(node->rhs, src))
                return;
            _chunk->emit<SetIndex>(objReg, idxReg, src);
            freeRegister(idxReg);
            freeRegister(objReg);
            freeRegister(src);
            optReg = src;
            return;
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

        FuncMeta *getFuncMeta{};
        if(node->getter)
            getFuncMeta = generateFuncMeta(node->getter->parameters, node->getter->body);

        auto *propMeta = _rt.createCompilationObject<PropMeta>(setFuncMeta, getFuncMeta);

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
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
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
                _chunk->emit<DLocal>(src, variable->reg);
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
        // 每个未初始化局部变量必须持有独立寄存器并实体化 void：
        // 共享 loadVoidReg() 会让 var a, b, c; 别名同一槽位，后续赋值互相覆盖
        const Register voidReg = allocateRegister();
        _chunk->emit<Load>(voidReg, ConstIdx{ 0 });
        addLocalVar(LocalVariable{ identifier, voidReg, makeLabel() });
    }

    void IRGenerator::generate(const syntax::FunctionDeclNode *node, OptReg &) {
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
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
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
        const auto identifier = getAtomFromToken(node->token);

        auto classReg = allocateRegister();
        auto *classMeta = _rt.createCompilationObject<ClassMeta>(identifier, 0);
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

            FuncMeta *getFuncMeta{};
            if(propertyDeclNode->getter)
                getFuncMeta = generateFuncMeta(propertyDeclNode->getter->parameters, propertyDeclNode->getter->body);

            auto *propMeta = _rt.createCompilationObject<PropMeta>(setFuncMeta, getFuncMeta);

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
            if(!funChunk) {
                return; // parseAst(nullptr) 不应失败,防御性判空(空 Opt 解引用是 UB)
            }

            auto *chunk = _rt.createCompilationObject<vm::Chunk>(std::move(*funChunk));
            auto *initFuncMeta = _rt.createCompilationObject<FuncMeta>(0, chunk, std::move(gen._localVars));
            classMeta->initDefaultVal = initFuncMeta;
        }

        {
            auto gen = IRGenerator{ _r, _rt, _sourceFile };
            gen.makeVirtualGlobalScope();

            // must init var in begin state
            if(node->constructor) {
                // 与 generateFuncMeta 相同：实参按位置落入帧寄存器，
                // 默认参数守卫在全部参数分配后发射
                Vec<std::pair<Register, const syntax::ExprNode *>> defaultParams{};
                bool hasCollapse = false;
                u32 collapseBase = 0;
                Register collapseReg{ 0 };
                {
                    u32 paramIndex = 0;
                    for(const auto &param : node->constructor->parameters) {
                        const auto varName = param.name.type() == syntax::TokenType::None
                            ? _rt.atomTable.intern(String{})
                            : getAtomFromToken(param.name);
                        const Register paramReg = gen.allocateRegister();
                        if(param.collapse) {
                            hasCollapse = true;
                            collapseBase = paramIndex;
                            collapseReg = paramReg;
                        }
                        gen.addLocalVar(LocalVariable{ varName, paramReg, gen.makeLabel() });
                        if(param.defaultValue)
                            defaultParams.emplace_back(paramReg, param.defaultValue);
                        ++paramIndex;
                    }
                }
                gen.unnamedArgBase =
                    hasCollapse ? collapseBase : static_cast<u32>(node->constructor->parameters.size());

                // collapse 参数收拢（旧版 FuncDeclCollapseBase）：本帧实际实参 %base..
                // 收拢成 Array 落收拢参数寄存器；须在默认参数守卫前发射
                if(hasCollapse)
                    gen._chunk->emitRaw<CollapseArgs>(collapseReg, Integer{ collapseBase });

                for(const auto &[paramReg, exprNode] : defaultParams) {
                    // 仅当实参为 void(未传)时才以默认值覆盖
                    const Register voidFlag = gen.allocateRegister();
                    gen._chunk->emit<TestVoid>(paramReg, voidFlag);
                    gen._chunk->emit<Test>(voidFlag);
                    const size_t jmpIdx = gen._chunk->emit<NOP>();
                    Register defaultParamReg{ 0 };
                    if(!gen.expectValue(exprNode, defaultParamReg)) {
                        return;
                    }
                    gen._chunk->emit<DLocal>(defaultParamReg, paramReg);
                    const auto skipLabel = gen.makeLabel();
                    gen._chunk->repl<JmpNE>(jmpIdx, skipLabel);
                    gen.freeRegister(voidFlag);
                    gen.freeRegister(defaultParamReg);
                }
            }


            if(node->constructor) {
                OptReg ignoreReg{};
                auto funChunk = gen.parseAst(node->constructor->body, ignoreReg);
                if(!funChunk) {
                    return; // 函数体生成失败(错误已记录在 _r),调用方以 nullptr 处理
                }

                // the last patch one ret
                auto voidReg = gen.loadVoidReg();
                funChunk->emit<Ret>(voidReg);

                auto *chunk = _rt.createCompilationObject<vm::Chunk>(std::move(*funChunk));
                const size_t paramCount = node->constructor->parameters.size();
                auto *initFuncMeta =
                    _rt.createCompilationObject<FuncMeta>(static_cast<u32>(paramCount), chunk, std::move(gen._localVars));
                initFuncMeta->name = identifier;
                classMeta->setConstructor(initFuncMeta);
            }
        }

        // finalize function placeholder
        if(Atom finalizeAtom = _rt.atomTable.intern("finalize"_str); classMeta->hasMember(finalizeAtom) < 0) {
            OptReg ignoreReg{};
            IRGenerator genFinalize{ _r, _rt, _sourceFile };
            genFinalize.makeVirtualGlobalScope();
            auto voidReg = genFinalize.loadVoidReg();
            genFinalize._chunk->emit<Ret>(voidReg);
            auto funChunk = _rt.createCompilationObject<vm::Chunk>(*genFinalize.parseAst(nullptr, ignoreReg));
            auto *finalize = _rt.createCompilationObject<FuncMeta>(0, funChunk, Vec<LocalVariable>{});
            finalize->syntheticFinalizer = true;
            classMeta->setMember(MemberShapeMeta{ .name = finalizeAtom, .isMethod = true }, ClassFieldMeta{ finalize });
        }
        endScope();
        freeRegister(classReg);
    }

    void IRGenerator::generate(const syntax::IdentifierExprNode *node, OptReg &optReg) {
        auto dst = allocateRegister();
        const auto identifier = getAtomFromToken(node->token);

        if(const auto variable = resolveLocalVariable(identifier)) {
            _chunk->emit<GLocal>(variable->reg, dst);
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

    void IRGenerator::emitFunctionHoisting(const syntax::BlockStmtNode *node) {
        if(!node)
            return;
        OptReg ignore{};
        for(const auto children : node->childrens) {
            if(const auto *fn = dynamic_cast<const syntax::FunctionDeclNode *>(children)) {
                fn->generateBytecode(this, ignore);
            }
        }
    }

    void IRGenerator::generate(const syntax::BlockStmtNode *node, OptReg &) {
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
        // block statement never return value
        beginScope();
        OptReg ignore{};
        // TJS 语义: 函数声明在块内任何代码执行前注册 (hoisting),
        // 允许先调用后声明的写法 (test_misc.tjs 第 1-2 行即此形态)
        emitFunctionHoisting(node);
        for(const auto children : node->childrens) {
            if(dynamic_cast<const syntax::FunctionDeclNode *>(children))
                continue; // 已提升, 避免重复注册
            children->generateBytecode(this, ignore);
        }
        endScope();
    }

    void IRGenerator::generate(const syntax::IfStmtNode *node, OptReg &) {
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
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
            // then 分支执行完应无条件跳过 else 分支；
            // 此处曾是 JmpNE（依赖陈旧的 ZF，导致真假两个分支都执行）
            _chunk->repl<Jmp>(jmpIdx, makeLabel());
        }

        freeRegister(testReg);
    }

    void IRGenerator::generate(const syntax::SwitchStmtNode *node, OptReg &) {
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
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
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
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
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
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
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
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
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
        if(_breakStack.empty()) {
            error("break keyword must in loop or switch scope", node->location);
            return;
        }
        const size_t idx = _chunk->emit<NOP>();
        _breakStack.back().push_back(idx);
    }

    void IRGenerator::generate(const syntax::ContinueStmtNode *node, OptReg &) {
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
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

        _chunk->emit<DLocal>(lhsReg, dst);

        const size_t jmpIdx = _chunk->emit<NOP>();
        _chunk->repl<JmpNE>(jmpNeIdx, makeLabel());

        Register rhsReg{ 0 };
        if(!expectValue(node->rhsExpr, rhsReg)) {
            return;
        }

        _chunk->emit<DLocal>(rhsReg, dst);

        _chunk->repl<Jmp>(jmpIdx, makeLabel());
        freeRegister(testReg);
        optReg = dst;
    }

    void IRGenerator::generate(const syntax::IndexExprNode *node, OptReg &optReg) {
        Register objReg{ 0 };
        if(!expectValue(node->lhs, objReg))
            return;
        Register idxReg{ 0 };
        if(!expectValue(node->index, idxReg))
            return;
        Register dst = allocateRegister();
        _chunk->emit<GetIndex>(objReg, idxReg, dst);
        freeRegister(idxReg);
        freeRegister(objReg);
        optReg = dst;
    }

    void IRGenerator::generate(const syntax::ArrayLiteralNode *node, OptReg &optReg) {
        Register dst = allocateRegister();
        _chunk->emit<NewArray>(dst, static_cast<Integer>(node->elements.size()));
        for(size_t i = 0; i < node->elements.size(); ++i) {
            auto *elm = node->elements[i];
            if(!elm)
                continue; // 空元素保持 void
            Register valReg{ 0 };
            if(!expectValue(elm, valReg))
                return;
            Register idxReg = allocateRegister();
            _chunk->emit<LoadImm>(idxReg, static_cast<Integer>(i));
            _chunk->emit<SetIndex>(dst, idxReg, valReg);
            freeRegister(idxReg);
            freeRegister(valReg);
        }
        optReg = dst;
    }

    void IRGenerator::generate(const syntax::DictLiteralNode *node, OptReg &optReg) {
        Register dst = allocateRegister();
        _chunk->emit<NewDict>(dst);
        for(const auto &[keyExpr, valueExpr] : node->pairs) {
            Register keyReg{ 0 };
            if(!expectValue(keyExpr, keyReg))
                return;
            Register valReg{ 0 };
            if(!expectValue(valueExpr, valReg))
                return;
            _chunk->emit<SetIndex>(dst, keyReg, valReg);
            freeRegister(keyReg);
            freeRegister(valReg);
        }
        optReg = dst;
    }

    void IRGenerator::generate(const syntax::RegexLiteralExprNode *node, OptReg &optReg) {
        Register dst = allocateRegister();
        // 模式文本按字符串字面量同路径入常量池（Atom 形态）
        _chunk->emit<NewRegExp>(dst, _chunk->addConstant(getAtomFromToken(node->token)));
        optReg = dst;
    }

    void IRGenerator::generate(const syntax::WithBaseExprNode *node, OptReg &optReg) {
        Register dst = allocateRegister();
        _chunk->emit<GetWith>(dst);
        optReg = dst;
    }

    void IRGenerator::generate(const syntax::WithStmtNode *node, OptReg &) {
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
        // with(obj) stmt：obj 求值一次压入 with 上下文栈，块结束弹出；
        // 异常穿越时随帧截断（freeCallFrame）
        Register objReg{ 0 };
        if(!expectValue(node->expr, objReg))
            return;
        _chunk->emit<WithPush>(objReg);
        freeRegister(objReg);

        OptReg ignore{};
        node->body->generateBytecode(this, ignore);

        _chunk->emit<WithPop>();
    }

    void IRGenerator::generate(const syntax::ReturnStmtNode *node, OptReg &) {
        _chunk->addLine(node->location); // Rlc 行号（语句粒度）
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

        // 调用约定：实参按位置落入帧寄存器 0..arity-1，参数寄存器必须连续分配，
        // 默认参数的守卫代码只能在全部参数分配完之后发射
        Vec<std::pair<Register, const syntax::ExprNode *>> defaultParams{};
        bool hasCollapse = false;
        u32 collapseBase = 0;
        Register collapseReg{ 0 };
        {
            u32 paramIndex = 0;
            for(const auto &param : parameters) {
                // 未命名 collapse（裸 `*`）名为空 Token：getString 解引用未初始化
                // 载荷会段错误，落一个不会被引用的占位名
                const auto varName = param.name.type() == syntax::TokenType::None ? _rt.atomTable.intern(String{})
                                                                                  : getAtomFromToken(param.name);
                const Register paramReg = gen.allocateRegister();
                if(param.collapse) {
                    // 收拢参数（`name*` / `*`，语法上必为最后一个）：
                    // 未命名基 = 其位次（= 前面具名参数个数）
                    hasCollapse = true;
                    collapseBase = paramIndex;
                    collapseReg = paramReg;
                }
                gen.addLocalVar(LocalVariable{ varName, paramReg, gen.makeLabel() });
                if(param.defaultValue)
                    defaultParams.emplace_back(paramReg, param.defaultValue);
                ++paramIndex;
            }
        }
        // `*` 展开实参转发本帧实际实参的起点（旧版 FuncDeclUnnamedArgArrayBase）
        gen.unnamedArgBase = hasCollapse ? collapseBase : static_cast<u32>(parameters.size());

        // collapse 参数收拢（旧版 FuncDeclCollapseBase）：本帧实际实参 %base..
        // 收拢成 Array 落收拢参数寄存器；须在默认参数守卫前发射
        if(hasCollapse)
            gen._chunk->emitRaw<CollapseArgs>(collapseReg, Integer{ collapseBase });

        for(const auto &[paramReg, exprNode] : defaultParams) {
            // 默认参数语义（TJS2）：仅当对应实参为 void(未传)时才以默认值覆盖，
            // 否则实参已占据参数寄存器。降糖为 TestVoid + 条件跳过
            const Register voidFlag = gen.allocateRegister();
            gen._chunk->emit<TestVoid>(paramReg, voidFlag);
            gen._chunk->emit<Test>(voidFlag);
            const size_t jmpIdx = gen._chunk->emit<NOP>();
            Register defaultParamReg{ 0 };
            if(!gen.expectValue(exprNode, defaultParamReg)) {
                return nullptr;
            }
            gen._chunk->emit<DLocal>(defaultParamReg, paramReg);
            const auto skipLabel = gen.makeLabel();
            gen._chunk->repl<JmpNE>(jmpIdx, skipLabel);
            gen.freeRegister(voidFlag);
            gen.freeRegister(defaultParamReg);
        }

        OptReg ignoreReg{};
        auto funChunk = gen.parseAst(body, ignoreReg);
        // 函数体含不支持语法(如前缀 &/*)时 parseAst 因 _r 失败而返回空 Opt;
        // 旧代码在此 assert——Debug 中止、Release 对空 Opt 解引用段错误。
        // 返回 nullptr 交给调用方走既有的优雅报错路径
        if(!funChunk) {
            return nullptr;
        }

        // the last instruction is not ret, patch one ret
        if(auto &instVec = funChunk->getInstVec(); instVec.empty() || instVec.back()->opcode() != TacOpCode::Ret) {
            funChunk->emit<Ret>(gen.loadVoidReg());
        }

        auto *chunk = _rt.createCompilationObject<vm::Chunk>(std::move(*funChunk));
        auto *funcMeta =
            _rt.createCompilationObject<FuncMeta>(static_cast<u32>(parameters.size()), chunk, std::move(gen._localVars));
        funcMeta->hasCollapse = hasCollapse;
        return funcMeta;
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
            case syntax::TokenValueType::Octet: {
                // octet 字面量走常量池(payload 所有权随 Constant)
                auto *payload = SharedPayload<Octet>::makeSingle(Octet{ token.getOctet() });
                _chunk->emit<Load>(reg, _chunk->addConstant(Constant{ payload }));
                break;
            }
            case syntax::TokenValueType::Integer:
                _chunk->emit<LoadImm>(reg, token.getInteger());
                break;
            case syntax::TokenValueType::None:
                // void 值（void 字面量 / 缺省 else 分支）：常量池 0 号约定为 Void。
                // 旧实现什么都不发射，目标寄存器保留陈旧值——`x === void` 等比较
                // 因此读到垃圾而非 void
                _chunk->emit<Load>(reg, ConstIdx{ 0 });
                break;
        }
    }

} // namespace cial::inter
