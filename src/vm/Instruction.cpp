/*
 * Copyright (c) 2024/6/12 下午8:44
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
#include "Instruction.hpp"
#include <fmt/format.h>

#include "VMDebug.hpp"
#include "VMState.hpp"
#include "logging/Logger.hpp"
#include "types/Class.hpp"
#include "types/Function.hpp"
#include "types/Object.hpp"
#include "vm/Register.hpp"

namespace cial::Bytecode::Op {

    void Instruction::execute(const Instruction &inst, VMState &vmState) {
#define HANDLE_OPCODE(OP)                                                                                              \
    case OpCode::OP:                                                                                                   \
        OP::execute(inst, vmState);                                                                                    \
        break;
        switch(inst.opcode) {
            OPCODE_ENUMS(HANDLE_OPCODE)
            default:;
        }
    }

    std::string Instruction::dump(const Instruction &inst, const VMState *vmState) {
#define DUMP_OPCODE(OP)                                                                                                \
    case OpCode::OP:                                                                                                   \
        return OP::dump(inst, vmState);
        switch(inst.opcode) {
            OPCODE_ENUMS(DUMP_OPCODE)
            default:;
        }
        return "";
    }

    void Load::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(reg(inst), vmState.curFrame()->chunk->getConstant(value(inst)).createValue(&vmState.rt));
    }

    void PushReg::execute(const Instruction &inst, VMState &vmState) { vmState.push(vmState.reg(src(inst))); }

    void PopN::execute(const Instruction &inst, VMState &vmState) { vmState.pop(count(inst)); }

    void CP::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst), vmState.reg(src(inst)));
    }

    void Add::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(reg1(inst));
        const Value r2 = vmState.reg(reg2(inst));
        auto r = r1.add(r2);
        vmState.reg(dst(inst), r.unwrap());
    }

    void Sub::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(reg1(inst));
        const Value r2 = vmState.reg(reg2(inst));
        auto r = r1.sub(r2);
        vmState.reg(dst(inst), r.unwrap());
    }

    void Mul::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(reg1(inst));
        const Value r2 = vmState.reg(reg2(inst));
        auto r = r1.mul(r2);
        vmState.reg(dst(inst), r.unwrap());
    }

    void Div::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(reg1(inst));
        const Value r2 = vmState.reg(reg2(inst));
        auto r = r1.div(r2);
        vmState.reg(dst(inst), r.unwrap());
    }

    void Mov::execute(const Instruction &inst, const VMState &vmState) {
        const Value &srcVal = vmState.reg(src(inst));
        vmState.reg(dst(inst), srcVal);
    }

    void DGlobal::execute(const Instruction &inst, const VMState &vmState) {
        const auto &value = vmState.reg(src(inst));
        vmState.global(atom(inst), Value{ value });
    }

    void GGlobal::execute(const Instruction &inst, const VMState &vmState) {
        const auto &value = vmState.global(atom(inst));
        vmState.reg(dst(inst), value);
    }

    void Test::execute(const Instruction &inst, VMState &vmState) {
        if(vmState.reg(reg(inst)).asBool()) {
            vmState.setZF(true);
        }
    }

    void EQ::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = value1.equals(value2);
        vmState.reg(dst(inst), Value{ result });
        vmState.setZF(result);
    }

    void NEQ::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = !value1.equals(value2);
        vmState.reg(dst(inst), Value{ result });
        vmState.setZF(result);
    }

    void LT::execute(const Instruction &inst, VMState &vmState) {
        const auto &kIpt = static_cast<const VMState &>(vmState);
        const auto &value1 = kIpt.reg(reg1(inst));
        const auto &value2 = kIpt.reg(reg2(inst));
        const bool result = value1.littlerThan(value2).unwrap();
        vmState.reg(dst(inst), Value{ result });
        vmState.setZF(result);
    }

    void LE::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = !value1.greaterThan(value2).unwrap();
        vmState.reg(dst(inst), Value{ result });
        vmState.setZF(result);
    }

    void GT::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = value1.greaterThan(value2).unwrap();
        vmState.reg(dst(inst), Value{ result });
        vmState.setZF(result);
    }

    void GE::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = !value1.littlerThan(value2).unwrap();
        vmState.reg(dst(inst), Value{ result });
        vmState.setZF(result);
    }

    void LAnd::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = value1.logicalAnd(value2);
        vmState.reg(dst(inst), Value{ result });
        vmState.setZF(result);
    }

    void LOr::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = value1.logicalOr(value2);
        vmState.reg(dst(inst), Value{ result });
        vmState.setZF(result);
    }

    void AbsEQ::execute(const Instruction &, VMState &) {
        // TODO:
        assert(false);
    }

    void Jmp::execute(const Instruction &inst, VMState &vmState) { vmState.setPC(label(inst)); }

    void JmpE::execute(const Instruction &inst, VMState &vmState) {
        if(vmState.getZF()) {
            vmState.setPC(label(inst));
        }
    }

    void JmpNE::execute(const Instruction &inst, VMState &vmState) {
        if(!vmState.getZF()) {
            vmState.setPC(label(inst));
        }
    }

    void Call::execute(const Instruction &inst, VMState &vmState) {
        const auto &object = vmState.reg(memberReg(inst));
        object.asObject().unwrap()->call(vmState, dst(inst), argCount(inst));
    }

    void GProp::execute(const Instruction &inst, const VMState &vmState) {
        const auto &val = vmState.reg(obj(inst));
        const String &name = *vmState.reg(memberReg(inst)).asString().unwrap();
        const Atom atom = vmState.rt.atomTable.intern(name);
        if(val.isObject()) {
            if(auto *instObj = dynamic_cast<InstanceObject *>(val.asObject().value())) {
                const auto tmp = instObj->getProp(atom);
                vmState.reg(dst(inst), tmp);
                return;
            }
        }
        const TypeId tId = ValueToTypeId::getId(val);
        assert(tId != TypeId::None);
        NativeFunction *nativeFn = vmState.context.findMethod(tId, atom);
        assert(nativeFn != nullptr);
        nativeFn = vmState.rt.create<NativeFunction>(*nativeFn);
        nativeFn->setThisObj(val);
        vmState.reg(dst(inst), Value{ nativeFn });
    }

    const String *SProp::name(const Instruction &inst, const VMState &vmState) {
        if(inst.getOperand2Type() == Operand::Type::Atom)
            return vmState.rt.atomTable.get(inst.getOperand2<Atom>())->str;
        if(inst.getOperand2Type() == Operand::Type::Register)
            return vmState.reg(inst.getOperand2<Register>()).asString().unwrap();
        CLL_ASSERT(false, "unknown inst sprop operand2 type");
    }

    Value SProp::value(const Instruction &inst, const VMState &vmState) {
        if(inst.getOperand3Type() == Operand::Type::ConstIndex)
            return vmState.curFrame()->chunk->getConstant(inst.getOperand3<ConstIdx>()).createValue(&vmState.rt);
        if(inst.getOperand3Type() == Operand::Type::Register)
            return vmState.reg(inst.getOperand3<Register>());
        CLL_ASSERT(false, "unknown inst sprop operand3 type");
    }

    void SProp::execute(const Instruction &inst, VMState &vmState) {
        // TODO:
        // const auto &instObj = vmState.reg(obj(inst));
        // CLL_ASSERT(instObj.isObject(), "gprop obj is not object");
        //
        // if(auto *inst = dynamic_cast<ClassObject *>(instObj.toObject())) {
        //     inst->set(name(inst, vmState), value(inst, vmState));
        // }
        throw std::runtime_error("not implemented");
    }


    void GThis::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst), vmState.getThis(atom(inst)));
    }

    void GUpval::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst), vmState.getUpVal(atom(inst)));
    }

    void LNot::execute(const Instruction &inst, const VMState &vmState) {
        const Register srcReg = src(inst);
        vmState.regRef(srcReg).toLogicalNot();
    }

    void ChS::execute(const Instruction &inst, const VMState &vmState) {
        auto r = vmState.regRef(src(inst)).toSignChange();
        if(r.isFailed())
            throw r.getErr();
    }

    void Ret::execute(const Instruction &inst, VMState &vmState) {
        const auto &value = vmState.reg(retReg(inst));
        const auto frame = vmState.curFrame();
        CLL_ASSERT(frame->ret, "frame.ret val is empty");
        vmState.prev()->getReg(*frame->ret) = value;
        vmState.freeCallFrame();
    }

    std::string Load::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4}", "load", reg(inst), value(inst));
    }

    std::string PushReg::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4}", "pushreg", src(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, src(inst), vmState->reg(src(inst)));
    }

    std::string PopN::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "popn", count(inst));
    }
    std::string CP::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "cp", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, src(inst), vmState->reg(src(inst)));
    }

    std::string Add::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "add", reg1(inst), reg2(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, reg1(inst), vmState->reg(reg1(inst)), reg2(inst),
                           vmState->reg(reg2(inst)));
    }


    std::string Sub::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "sub", reg1(inst), reg2(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, reg1, vmState->reg(reg1(inst)), reg2(inst),
                           vmState->reg(reg2(inst)));
    }
    std::string Mul::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "mul", reg1(inst), reg2(inst), dst(inst));
    }

    std::string Div::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "div", reg1(inst), reg2(inst), dst(inst));
    }

    std::string Mov::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4}", "mov", src(inst), dst(inst));
    }

    std::string DGlobal::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} atom_{}", "dglobal", src(inst), atom(inst).v);

        if(!vmState)
            return insDump;
        const auto *aEntry = vmState->rt.atomTable.get(atom(inst));

        return fmt::format("{: <30};{}={} atom_{}=\"{}\"", insDump, src(inst), vmState->reg(src(inst)), atom(inst).v,
                           *aEntry->str);
    }

    std::string GGlobal::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} atom_{} {: <4}", "gglobal", atom(inst).v, dst(inst));
    }

    std::string Test::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "test", reg(inst));
    }

    std::string EQ::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "eq", reg1(inst), reg2(inst), dst(inst));
    }

    std::string NEQ::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}, {: <4}, {: <4}", "neq", reg1(inst), reg2(inst), dst(inst));
    }

    std::string LT::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "lt", reg1(inst), reg2(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, reg1(inst), vmState->reg(reg1(inst)), reg2(inst),
                           vmState->reg(reg2(inst)));
    }

    std::string LE::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "le", reg1(inst), reg2(inst), dst(inst));
    }
    std::string GT::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "ge", reg1(inst), reg2(inst), dst(inst));
    }

    std::string GE::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "ge", reg1(inst), reg2(inst), dst(inst));
    }

    std::string LAnd::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "land", reg1(inst), reg2(inst), dst(inst));
    }

    std::string LOr::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "lor", reg1(inst), reg2(inst), dst(inst));
    }

    std::string AbsEQ::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "abseq", reg1(inst), reg2(inst), dst(inst));
    }

    std::string Jmp::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "jmp", label(inst));
    }

    std::string JmpE::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4}", "jmpe", label(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; ZF = {}", insDump, vmState->getZF());
    }

    std::string JmpNE::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4}", "jmpne", label(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; ZF = {}", insDump, vmState->getZF());
    }

    std::string Call::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "call", memberReg(inst), dst(inst), argCount(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, memberReg(inst), vmState->reg(memberReg(inst)));
    }

    std::string GProp::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "gprop", obj(inst), memberReg(inst), dst(inst));
    }

    std::string SProp::dump(const Instruction &inst, const VMState *vmState) {
        // TODO:
        throw std::runtime_error("not implemented");
    }

    std::string GThis::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} atom_{} {: <4}", "gthis", atom(inst).v, dst(inst));
    }

    std::string GUpval::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} atom_{} {: <4}", "gupval", atom(inst).v, dst(inst));
    }

    std::string LNot::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "lnot", src(inst));
    }

    std::string ChS::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "chs", src(inst));
    }

    std::string Ret::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {}", "ret", retReg(inst));
    }

} // namespace cial::Bytecode::Op
