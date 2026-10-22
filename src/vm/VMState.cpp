/*
 * Copyright (c) 2024/5/8 上午8:08
 *   /\  _` \   __          /\_ \  /\_ \
 *   \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *    \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *     \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *      \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *       \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                               /\____/
 *                                                               \_/__/
 *
 */

#include "VMState.hpp"

#include "../gen/Instruction.hpp"
#include "VMDebug.hpp"
#include "stdlib/NativeRegister.hpp"
#include "types/Class.hpp"
#include "types/Function.hpp"
#include "types/Property.hpp"

namespace cial {
    void CallFrame::setThisObj(Value v) {
        thisObj = std::move(v);
        thisData = thisObj.isObject() ? dynamic_cast<DataObject *>(thisObj.asObject().value()) : nullptr;
    }
} // namespace cial

namespace cial::vm {

    [[nodiscard]] bool VMState::globalHas(const Atom atom) const { return context.global()->hasProp(atom); }

    [[nodiscard]] bool VMState::globalHas(const std::string &name) const {
        const auto atom = rt.atomTable.intern(name.c_str(), name.length());
        return context.global()->hasProp(atom);
    }

    [[nodiscard]] Value VMState::global(const Atom atom) const { return context.global()->getProp(atom); }

    void VMState::global(const Atom atom, const Value &value) const { context.global()->setProp(atom, value); }

    [[nodiscard]] Value VMState::global(const std::string &name) const {
        const auto atom = rt.atomTable.intern(name.c_str(), name.length());
        return context.global()->getProp(atom);
    }

    void VMState::global(const std::string &name, const Value &value) const {
        const auto atom = rt.atomTable.intern(name.c_str(), name.length());
        context.global()->setProp(atom, value);
    }

    const Value &VMState::reg(const u16 reg) const { return _curFrame->getReg(reg); }

    Value &VMState::regRef(const u16 reg) const { return _curFrame->getReg(reg); }

    bool VMState::hasThis(const Atom atom) const {

        // current thisObj（thisData 为帧创建时缓存的 DataObject*）
        if(_curFrame->thisData) {
            if(_curFrame->thisData->hasProp(atom)) {
                return true;
            }
        }

        if(auto *callFrame = _curFrame->closure; callFrame) {
            if(callFrame->funcMeta) {
                for(const auto &localVar : callFrame->funcMeta->localVars) {
                    if(localVar.endPC.address() > callFrame->pc)
                        continue;
                    if(localVar.identifier == atom)
                        return true;
                }
            }
        }

        // global
        return globalHas(atom);
    }

    Value VMState::getThis(const Atom atom) const {

        // current thisObj（thisData 为帧创建时缓存的 DataObject*）
        if(_curFrame->thisData) {
            if(_curFrame->thisData->hasProp(atom)) {
                return _curFrame->thisData->getProp(atom);
            }
        }

        if(auto *callFrame = _curFrame->closure; callFrame) {
            if(callFrame->funcMeta) {
                for(const auto &[identifier, reg, startPC, endPC] : callFrame->funcMeta->localVars) {
                    if(startPC.address() <= callFrame->pc && callFrame->pc < endPC.address())
                        continue;
                    if(identifier == atom)
                        return callFrame->getReg(reg.index());
                }
            }
        }

        // global
        return global(atom);
    }

    void VMState::setThis(const Atom atom, const Value &v) const {
        // current thisObj（thisData 为帧创建时缓存的 DataObject*）
        if(_curFrame->thisData) {
            if(_curFrame->thisData->hasProp(atom)) {
                _curFrame->thisData->setProp(atom, v);
                return;
            }
        }

        if(auto *callFrame = _curFrame->closure; callFrame) {
            if(callFrame->funcMeta) {
                for(const auto &localVar : callFrame->funcMeta->localVars) {
                    if(localVar.endPC.address() > callFrame->pc)
                        continue;
                    if(localVar.identifier == atom)
                        callFrame->getReg(localVar.reg.index()) = v;
                }
            }
        }

        // global
        return global(atom, v);
    }

    Value VMState::getUpVal(const Atom atom) const {

        // current context
        if(_curFrame->thisData) {
            return _curFrame->thisData->getProp(atom);
        }

        for(u16 i = _stackTop - 1; i > 0; --i) {
            const auto &callFrame = _callStack[i - 1];
            // prev local scope
            if(callFrame.funcMeta) {
                for(const auto &localVar : callFrame.funcMeta->localVars) {
                    if(localVar.endPC.address() > callFrame.pc)
                        continue;
                    if(localVar.identifier == atom)
                        return callFrame.getReg(localVar.reg.index());
                }
            }

            // prev context
            if(callFrame.thisObj.isObject()) {
                if(auto *instanceObject = dynamic_cast<DataObject *>(_curFrame->thisObj.asObject().value())) {
                    return instanceObject->getProp(atom);
                }
            }
        }

        // global
        return global(atom);
    }

    void VMState::unwind() {
        while(true) {
            const CallFrame *cFrame = _curFrame;
            // run0 的 pc 是扁平字节码偏移，因此这里查询字节码空间的 try 区间表
            const Opt<ThrowHandler> th = cFrame->chunk->findBcThrowHandler(getPC());
            if(!th) {
                if(cFrame->ret)
                    prevFrame()->getReg(*cFrame->ret) = Value{};
                freeCallFrame();
                if(_stackTop == 0) {
                    throw std::runtime_error(fmt::format("{}", _exValue));
                }
                continue;
            }

            if(th->exValueReg)
                regRef(th->exValueReg) = _exValue;

            setPC(th->tryEnd);
            clearException();
            return;
        }
    }

    static bool propObjectSet(VMState &vmState, const Value &src, const Value &dst) {
        if(dst.isObject()) {
            if(const auto *prop = dynamic_cast<Property *>(dst.asObject().value())) {
                prop->invokeSet(vmState, src);
                return true;
            }
        }
        return false;
    }

    static bool propObjectGet(VMState &vmState, const Value &src, Value &dst) {
        if(src.isObject()) {
            if(const auto object = src.asObject().unwrap()) {
                if(const auto *prop = dynamic_cast<Property *>(object)) {
                    dst = prop->invokeGet(vmState);
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * 扁平字节码解释器主循环。
     *
     * 嵌套进入（property getter / 类默认值初始化 / 构造函数）时，
     * entryStackTop 记录进入时的栈深：被调帧执行完毕（栈回落到入口之下）即返回，
     * 交还给触发嵌套调用的 dispatch 继续。
     */
    void VMState::run0() {
        const size_t entryStackTop = _stackTop;
        if(entryStackTop == 0)
            return;

        for(;;) {
        FETCH_NEW_FRAME:
            CallFrame *cur = _curFrame;
            u64 pc = cur->pc;
            Chunk *chunk = cur->chunk;
            // 字节码按需编译（幂等）：进入字节码路径的 chunk 才编译
            chunk->toBytecode();
            const Constant *constants = chunk->getConstants().data();
            Value *regs = cur->regs();

            const Vec<u8> &codeVec = chunk->code();
            const u8 *ip = codeVec.data();
            const size_t bcSize = codeVec.size();

            while(pc < bcSize) {
                DispatchRet r{};
                switch(static_cast<VmOpCode>(ip[pc++])) {
#define DISPATCH_CASE(op)                                                                                              \
    case VmOpCode::op:                                                                                                 \
        r = dispatch##op(cur, constants, regs, ip, pc);                                                                \
        break;
                    CIAL_BYTECODE_OPCODE_ENUMS(DISPATCH_CASE)
#undef DISPATCH_CASE
                }

                if(r == DispatchRet::Call) {
                    goto FETCH_NEW_FRAME;
                }

                if(r == DispatchRet::Return) {
                    break;
                }

                if(_pending == PendingCF::Throw) {
                    // 同步局部 pc 到帧上（unwind 依据它查找 try 区间）；
                    // unwind 可能释放当前帧及以上帧，之后以 _curFrame 为准重新取指
                    cur->pc = pc;
                    unwind();
                    if(_curFrame != cur) {
                        goto FETCH_NEW_FRAME;
                    }
                    pc = _curFrame->pc;
                }
            }

            // 内层循环退出：Return / 帧代码执行到末尾
            if(_stackTop > 1) {
                freeCallFrame();
                if(_stackTop < entryStackTop) {
                    return; // 嵌套进入：目标帧已完成，交还原调用方
                }
                continue; // 取父帧，从其保存的 pc 继续
            }
            break; // 最底层帧执行完毕
        }
    }

#define DISPATCH_OP_METHOD(op)                                                                                         \
    DispatchRet VMState::dispatch##op(CallFrame *cur, const Constant *constants, Value *regs, const u8 *ip, u64 &pc)

    DISPATCH_OP_METHOD(NOP) { return DispatchRet::Continue; }

    DISPATCH_OP_METHOD(Debugger) {
        std::string regsDump = dumpCurRegisters();
        std::string localVarDump = dumpCurLocalVars();
        std::string instructionsDump = dumpCurInstructions();
        std::string constantsDump = dumpCurConstants();

        fmt::println("========================");
        fmt::println("========= Dump =========");
        fmt::println("========================");
        fmt::println("========= reg =========");
        fmt::println("{}", regsDump);
        fmt::println("======= local var =======");
        fmt::println("{}", localVarDump);
        fmt::println("========= const =========");
        fmt::println("{}", constantsDump);
        fmt::println("========= inst =========");
        fmt::println("{}", instructionsDump);
        DEBUG_BREAK();
        return DispatchRet::Continue;
    }

    // ── 栈操作 ───────────────────────────────────────────────
    DISPATCH_OP_METHOD(Push) {
        const u16 r1 = read<u16>(ip, pc);
        push(regs[r1]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(PopN) {
        const i64 count = decodeSleb128(ip, pc, 8);
        pop(static_cast<size_t>(count));
        return DispatchRet::Continue;
    }

    // ── 上下文对象 ───────────────────────────────────────────
    DISPATCH_OP_METHOD(Global) {
        const u16 r1 = read<u16>(ip, pc);
        regs[r1] = Value{ context.global() };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Super) {
        const u16 r1 = read<u16>(ip, pc);
        regs[r1] = Value{ dynamic_cast<DataObject *>(cur->thisObj.asObject().unwrap())->getSuperClass() };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(This) {
        const u16 r1 = read<u16>(ip, pc);
        regs[r1] = Value{ cur->thisObj };
        return DispatchRet::Continue;
    }

    // ── 标志/失效 ────────────────────────────────────────────
    DISPATCH_OP_METHOD(Test) {
        const u16 r1 = read<u16>(ip, pc);
        _zf = regs[r1].asBool();
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Inv) {
        const u16 r1 = read<u16>(ip, pc);
        // invalidate 会把 finalize 帧压栈（如果类定义了 finalize）；
        // run0 缓存了当前帧，必须跳到新帧执行完再回来
        const size_t top = _stackTop;
        dynamic_cast<DataObject *>(regs[r1].asObject().unwrap())->invalidate();
        if(_stackTop != top) {
            cur->pc = pc;
            return DispatchRet::Call;
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Throw) {
        const u16 r1 = read<u16>(ip, pc);
        throwException(regs[r1]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Ret) {
        const u16 r1 = read<u16>(ip, pc);
        // 顶层帧（无父帧/无返回寄存器）直接结束，不回写
        if(_stackTop > 1 && cur->ret) {
            Value &value = regs[r1];
            Value &dst = (cur - 1)->getReg(*cur->ret);
            if(value.isIntegerFast()) {
                // 整数直写:跳过 Value 拷贝构造/析构(数值递归的返回主路径)
                dst.type(ValueType::Integer);
                dst.asIntegerStore(value.asIntegerUnsafe());
            } else {
                dst = value;
            }
        }
        return DispatchRet::Return;
    }

    // ── 跳转（u32 目标已在编译期回填） ───────────────────────
    DISPATCH_OP_METHOD(Jmp) {
        pc = read<u32>(ip, pc);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(JmpE) {
        const u32 addr = read<u32>(ip, pc);
        if(_zf)
            pc = addr;
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(JmpNE) {
        const u32 addr = read<u32>(ip, pc);
        if(!_zf)
            pc = addr;
        return DispatchRet::Continue;
    }

    // ── 类型转换/单目 ─────────────────────────────────────────
    DISPATCH_OP_METHOD(ToInt) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value &dst = regs[r2];
        dst = regs[r1];
        if(const auto r = dst.toInteger(); r.isFailed()) {
            throw r.getErr();
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ToReal) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value &dst = regs[r2];
        dst = regs[r1];
        if(const auto r = dst.toReal(); r.isFailed()) {
            throw r.getErr();
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ToString) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value &dst = regs[r2];
        dst = regs[r1];
        if(const auto r = dst.toString(); r.isFailed()) {
            throw r.getErr();
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(LNot) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value &dst = regs[r2];
        dst = regs[r1];
        dst.toLogicalNot();
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ChgSign) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value &dst = regs[r2];
        dst = regs[r1];
        if(const auto r = dst.toSignChange(); r.isFailed()) {
            throw r.getErr();
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ChgThis) {
        const u16 r1 = read<u16>(ip, pc); // 函数
        const u16 r2 = read<u16>(ip, pc); // 新 this
        dynamic_cast<Function *>(regs[r1].asObject().unwrap())->thisObj = regs[r2].asObject().unwrap();
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ChkInv) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        regs[r2] = Value{ dynamic_cast<DataObject *>(regs[r1].asObject().unwrap())->isValid() };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(ChkIns) {
        const u16 r1 = read<u16>(ip, pc); // dst（输入对象/输出 bool）
        const u16 r2 = read<u16>(ip, pc); // 类名字符串
        Value &dst = regs[r1];
        if(const Value &src = regs[r2]; src.isString()) {
            const Atom atom = rt.atomTable.intern(*src.asString().value());
            dst = Value{ Integer{ dst.asObject().unwrap()->instanceOf(atom) } };
            return DispatchRet::Continue;
        }
        dst = Value{ Integer{ false } };
        return DispatchRet::Continue;
    }

    // ── 装载 ─────────────────────────────────────────────────
    DISPATCH_OP_METHOD(Load) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 cIdx = read<u16>(ip, pc);
        regs[r1] = constants[cIdx].createValue(&rt);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(LoadImm) {
        const u16 r1 = read<u16>(ip, pc);
        const i64 imm = decodeSleb128(ip, pc, 8);
        regs[r1] = Value{ imm };
        return DispatchRet::Continue;
    }

    // ── 全局/this/局部变量访问 ────────────────────────────────
    // 属性 setter（Property::invokeSet）只压帧不执行，需要返回 Call
    // 让主循环跳到新帧；getter（invokeGet）内部嵌套 run0 执行完毕，不影响本帧。
    DISPATCH_OP_METHOD(DGlobal) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        const Atom a{ atom };
        const Value &srcVal = regs[r1];
        if(globalHas(a) && propObjectSet(*this, srcVal, global(a))) {
            cur->pc = pc;
            return DispatchRet::Call;
        }
        global(a, Value{ srcVal });
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(GGlobal) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        if(const Value *g = context.global()->findPropRef(Atom{ atom })) {
            // Property 触发 getter 的慢路径之外,直接从槽位零拷贝取值。
            // 注意寄存器写入可能触发 setter 语义无关,只读安全;
            // Property 调用可能修改 _props(重入),先拷出再降级慢路径
            if(g->isObject() && dynamic_cast<const Property *>(g->asObject().unwrap()) != nullptr) {
                Value srcVal = *g;
                propObjectGet(*this, srcVal, srcVal);
                regs[r1] = srcVal;
                return DispatchRet::Continue;
            }
            regs[r1] = *g;
            return DispatchRet::Continue;
        }
        throw std::runtime_error("in global object Not found property");
    }

    DISPATCH_OP_METHOD(GThis) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        Value srcVal = getThis({ atom });
        propObjectGet(*this, srcVal, srcVal);
        regs[r1] = srcVal;
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(DThis) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        const Atom a{ atom };
        const Value &srcVal = regs[r1];

        if(hasThis(a)) {
            if(propObjectSet(*this, srcVal, getThis(a))) {
                cur->pc = pc;
                return DispatchRet::Call;
            }
        }

        setThis(a, srcVal);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(GLocal) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value srcVal = regs[r1];
        propObjectGet(*this, srcVal, srcVal);
        regs[r2] = srcVal;
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(DLocal) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value srcVal = regs[r1];
        if(propObjectSet(*this, srcVal, regs[r2])) {
            cur->pc = pc;
            return DispatchRet::Call;
        }
        regs[r2] = srcVal;
        return DispatchRet::Continue;
    }

    // ── 算术/位运算（三寄存器） ───────────────────────────────
    // 整数特化：双 int 时绕开 Ret<Value> 装配（optional 构造/析构），
    // 直接写寄存器槽——循环体算术的主路径
#define DISPATCH_RRR_INT(op, optr, method) \
    DISPATCH_OP_METHOD(op) { \
        const u16 r1 = read<u16>(ip, pc); \
        const u16 r2 = read<u16>(ip, pc); \
        const u16 r3 = read<u16>(ip, pc); \
        Value &a = regs[r1]; \
        Value &b = regs[r2]; \
        if(a.isIntegerFast() && b.isIntegerFast()) { \
            regs[r3] = Value{ a.asIntegerUnsafe() optr b.asIntegerUnsafe() }; \
            return DispatchRet::Continue; \
        } \
        regs[r3] = (a.method(b)).unwrap(); \
        return DispatchRet::Continue; \
    }

#define DISPATCH_RRR(op, expr)                                                                                          \
    DISPATCH_OP_METHOD(op) {                                                                                            \
        const u16 r1 = read<u16>(ip, pc);                                                                               \
        const u16 r2 = read<u16>(ip, pc);                                                                               \
        const u16 r3 = read<u16>(ip, pc);                                                                               \
        regs[r3] = (expr);                                                                                              \
        return DispatchRet::Continue;                                                                                    \
    }

    DISPATCH_RRR_INT(Add, +, add)
    DISPATCH_RRR_INT(Sub, -, sub)
    DISPATCH_RRR_INT(Mul, *, mul)
    DISPATCH_RRR(Div, (regs[r1].div(regs[r2]).unwrap()))
    DISPATCH_RRR(Idiv, (regs[r1].idiv(regs[r2]).unwrap()))
    DISPATCH_RRR(Mod, (regs[r1].mod(regs[r2]).unwrap()))

    DISPATCH_RRR(BXor, (regs[r1].bitwiseXor(regs[r2]).unwrap()))
    DISPATCH_RRR(BOr, (regs[r1].bitwiseOr(regs[r2]).unwrap()))
    DISPATCH_RRR(BAnd, (regs[r1].bitwiseAnd(regs[r2]).unwrap()))
    DISPATCH_RRR(BlShift, (regs[r1].bitwiseLeftShift(regs[r2]).unwrap()))
    DISPATCH_RRR(BrShift, (regs[r1].bitwiseRightShift(regs[r2]).unwrap()))
    DISPATCH_RRR(BurShift, (regs[r1].bitwiseUnsignedRightShift(regs[r2]).unwrap()))

    // ── 比较/逻辑（结果为 bool） ──────────────────────────────
#define DISPATCH_CMP(op, expr)                                                                                          \
    DISPATCH_OP_METHOD(op) {                                                                                            \
        const u16 r1 = read<u16>(ip, pc);                                                                               \
        const u16 r2 = read<u16>(ip, pc);                                                                               \
        const u16 r3 = read<u16>(ip, pc);                                                                               \
        regs[r3] = Value{ (expr) };                                                                                     \
        return DispatchRet::Continue;                                                                                    \
    }

    DISPATCH_CMP(EQ, regs[r1].equals(regs[r2]))
    DISPATCH_CMP(NEQ, !regs[r1].equals(regs[r2]))
    DISPATCH_CMP(AbsEQ, regs[r1].discernEquals(regs[r2]))
    DISPATCH_CMP(AbsNEQ, !regs[r1].discernEquals(regs[r2]))
    DISPATCH_OP_METHOD(LT) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        const u16 r3 = read<u16>(ip, pc);
        const Value &a = regs[r1];
        const Value &b = regs[r2];
        if(a.isIntegerFast() && b.isIntegerFast()) {
            regs[r3] = Value{ Integer{ a.asIntegerUnsafe() < b.asIntegerUnsafe() } };
            return DispatchRet::Continue;
        }
        regs[r3] = Value{ a.littlerThan(b).unwrap() };
        return DispatchRet::Continue;
    }
    DISPATCH_CMP(LE, !regs[r1].greaterThan(regs[r2]).unwrap())
    DISPATCH_CMP(GT, regs[r1].greaterThan(regs[r2]).unwrap())
    DISPATCH_CMP(GE, !regs[r1].littlerThan(regs[r2]).unwrap())
    DISPATCH_CMP(LAnd, regs[r1].logicalAnd(regs[r2]))
    DISPATCH_CMP(LOr, regs[r1].logicalOr(regs[r2]))

    // ── 调用/属性 ────────────────────────────────────────────
    DISPATCH_OP_METHOD(Call) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        const i64 imm = decodeSleb128(ip, pc, 8);
        const auto &object = regs[r2].asObject().unwrap();
        cur->pc = pc;
        object->call(*this, r1, imm);
        return DispatchRet::Call;
    }

    DISPATCH_OP_METHOD(GProp) {
        const u16 r1 = read<u16>(ip, pc); // obj
        const u16 r2 = read<u16>(ip, pc); // member
        const u16 r3 = read<u16>(ip, pc); // dst
        const auto &val = regs[r1];
        const String &name = *regs[r2].asString().unwrap();
        const Atom atom = rt.atomTable.intern(name);

        if(val.isObject()) {
            auto *obj = val.asObject().value();

            if(auto *global = dynamic_cast<GlobalObject *>(obj)) {
                // for global, proxy object, maybe get extends object in dataObject?
                if(global->proxy) {
                    throw std::runtime_error("Not implemented");
                }
                regs[r3] = global->getProp(atom);
                return DispatchRet::Continue;
            }

            if(const auto *classObject = dynamic_cast<ClassObject *>(obj)) {
                auto tmp = obj->getProp(atom);
                propObjectGet(*this, tmp, tmp);
                if(tmp.isObject()) {
                    auto *fun = dynamic_cast<Function *>(tmp.asObject().value());
                    fun->thisObj = cur->thisObj.asObject().value();
                }
                regs[r3] = tmp;
                return DispatchRet::Continue;
            }

            auto tmp = obj->getProp(atom);
            propObjectGet(*this, tmp, tmp);
            regs[r3] = tmp;
            return DispatchRet::Continue;
        }

        // Native method
        const TypeId tId = ValueToTypeId::getId(val);
        assert(tId != TypeId::None);
        NativeFunction *nativeFn = context.findMethod(tId, atom);
        assert(nativeFn != nullptr);
        nativeFn = rt.create<NativeFunction>(*nativeFn).get();
        nativeFn->setThisObj(val);
        regs[r3] = Value{ nativeFn };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(DProp) {
        const u16 r1 = read<u16>(ip, pc); // obj
        const u16 r2 = read<u16>(ip, pc); // member
        const u16 r3 = read<u16>(ip, pc); // src
        const auto &objVal = regs[r1];
        CLL_ASSERT(objVal.isObject(), "dprop obj is not object");
        const String &name = *regs[r2].asString().unwrap();
        const Atom atom = rt.atomTable.intern(name);
        objVal.asObject().unwrap()->setProp(atom, regs[r3]);
        return DispatchRet::Continue;
    }

} // namespace cial::vm
