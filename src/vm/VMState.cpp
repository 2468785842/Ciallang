/*
 * Copyright (c) 2024/5/8 上午8:08
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */
#include "VMState.hpp"

#include "../gen/Instruction.hpp"
#include "types/Class.hpp"
#include "types/Property.hpp"

namespace cial::vm {

    void VMState::run() {
        const size_t curStackTop = _stackTop;
        for(;;) {
            u64 &pc = _curFrame->pc;
            const auto &instList = _curFrame->chunk->getInstVec();

            if(pc >= instList.size())
                break;

            if(_stackTop == 0)
                break;

            if(curStackTop > _stackTop)
                break;

            const auto &instruction = instList[pc];
            // fmt::println("{}\n", Instruction::dump(*instruction, this));
            instruction->execute(*this);
            switch(_pending) {
                case PendingCF::Throw:
                    unwind();
                    break;
                case PendingCF::None:
                    break;
            }
            ++pc;
        }
    }

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

    Value VMState::reg(const u16 reg) const { return _curFrame->getReg(reg); }

    Value &VMState::regRef(const u16 reg) const { return _curFrame->getReg(reg); }

    bool VMState::hasThis(const Atom atom) const {

        // current thisObj
        if(_curFrame->thisObj.isObject()) {
            if(auto *instanceObject = dynamic_cast<DataObject *>(_curFrame->thisObj.asObject().value())) {
                if(instanceObject->hasProp(atom)) {
                    return true;
                }
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

            // prev context
            // if(callFrame->thisObj.isObject()) {
            //     if(auto *instanceObject = dynamic_cast<InstanceObject *>(_currentFrame->thisObj.asObject().value()))
            //     {
            //         return instanceObject->getProp(atom);
            //     }
            // }
        }

        // global
        return globalHas(atom);
    }

    Value VMState::getThis(const Atom atom) const {

        // current thisObj
        if(_curFrame->thisObj.isObject()) {
            if(auto *instanceObject = dynamic_cast<DataObject *>(_curFrame->thisObj.asObject().value())) {
                if(instanceObject->hasProp(atom)) {
                    return instanceObject->getProp(atom);
                }
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

            // prev context
            // if(callFrame->thisObj.isObject()) {
            //     if(auto *instanceObject = dynamic_cast<InstanceObject *>(_currentFrame->thisObj.asObject().value()))
            //     {
            //         return instanceObject->getProp(atom);
            //     }
            // }
        }

        // global
        // TODO: check is exist
        return global(atom);
    }

    void VMState::setThis(const Atom atom, const Value &v) const {
        // current thisObj
        if(_curFrame->thisObj.isObject()) {
            if(auto *instanceObject = dynamic_cast<DataObject *>(_curFrame->thisObj.asObject().value())) {
                if(instanceObject->hasProp(atom)) {
                    instanceObject->setProp(atom, v);
                    return;
                }
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
        if(_curFrame->thisObj.isObject()) {
            if(auto *instanceObject = dynamic_cast<DataObject *>(_curFrame->thisObj.asObject().value())) {
                return instanceObject->getProp(atom);
            }
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
        // TODO: check is exist
        return global(atom);
    }

    void VMState::unwind() {
        while(true) {
            const CallFrame *cFrame = _curFrame;
            const Opt<ThrowHandler> th = cFrame->chunk->findThrowHandler(getPC());
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

    void VMState::run0() {
        if(_stackTop == 0)
            return;

        for(;;) {
        FETCH_NEW_FRAME:
            CallFrame *cur = _curFrame;
            u64 pc = cur->pc;
            Chunk *chunk = cur->chunk;
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

                if(_pending == PendingCF::Throw)
                    unwind();
            }

            if(_stackTop > 1)
                freeCallFrame();
            else if(pc >= bcSize)
                break;
        }
    }

#define DISPATCH_OP_METHOD(op)                                                                                         \
    DispatchRet VMState::dispatch##op(CallFrame *cur, const Constant *constants, Value *regs, const u8 *ip, u64 &pc)

    DISPATCH_OP_METHOD(NOP) { return DispatchRet::Continue; }

    DISPATCH_OP_METHOD(Push) {
        const u16 r1 = read<u16>(ip, pc);
        push(regs[r1]);
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Test) {
        const u16 r1 = read<u16>(ip, pc);
        _zf = regs[r1].asBool();
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Ret) {
        const u16 r1 = read<u16>(ip, pc);
        const Value &value = regs[r1];
        CLL_ASSERT(cur->ret, "frame.ret val is empty");
        (cur - 1)->getReg(*cur->ret) = value;
        return DispatchRet::Return;
    }

    DISPATCH_OP_METHOD(JmpNE) {
        const u32 addr = read<u32>(ip, pc);
        if(!_zf)
            pc = addr;
        return DispatchRet::Continue;
    }

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

    DISPATCH_OP_METHOD(DGlobal) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        const Atom a{ atom };
        const Value &srcVal = regs[r1];
        if(globalHas(a) && propObjectSet(*this, srcVal, global(a))) {
        } else {
            global(a, Value{ srcVal });
        }
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(GGlobal) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        Value srcVal = global(Atom{ atom });
        propObjectGet(*this, srcVal, srcVal);
        regs[r1] = srcVal;
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(GThis) {
        const u32 atom = read<u32>(ip, pc);
        const u16 r1 = read<u16>(ip, pc);
        Value srcVal = getThis({ atom });
        propObjectGet(*this, srcVal, srcVal);
        regs[r1] = srcVal;
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(DLocal) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        Value srcVal = regs[r1];
        if(!propObjectSet(*this, srcVal, regs[r2])) {
            regs[r2] = srcVal;
        }
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

    DISPATCH_OP_METHOD(Add) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        const Value r1v = regs[r1];
        Value &r2v = regs[r2];
        r2v = r1v.add(r2v).unwrap();
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Sub) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        const Value r1v = regs[r1];
        Value &r2v = regs[r2];
        r2v = r1v.sub(r2v).unwrap();
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(LT) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        const Value r1v = regs[r1];
        Value &r2v = regs[r2];
        const bool r = r1v.littlerThan(r2v).unwrap();
        r2v = Value{ r };
        return DispatchRet::Continue;
    }

    DISPATCH_OP_METHOD(Call) {
        const u16 r1 = read<u16>(ip, pc);
        const u16 r2 = read<u16>(ip, pc);
        const i64 imm = decodeSleb128(ip, pc, 8);
        const auto &object = regs[r2].asObject().unwrap();
        cur->pc = pc;
        object->call(*this, r1, imm);
        return DispatchRet::Call;
    }

} // namespace cial::vm
