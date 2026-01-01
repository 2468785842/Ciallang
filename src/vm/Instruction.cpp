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

namespace Cial::Bytecode::Op {

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
        VM_ASSERT(r1.isInteger() && r2.isInteger(), &vmState);
        vmState.reg(dst(inst), r1 + r2);
    }


    void Sub::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst), vmState.reg(reg1(inst)) - vmState.reg(reg2(inst)));
    }

    void Mul::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst), vmState.reg(reg1(inst)) * vmState.reg(reg2(inst)));
    }

    void Div::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst), vmState.reg(reg1(inst)) / vmState.reg(reg2(inst)));
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
        if(vmState.reg(reg(inst)).toBool()) {
            vmState.setZF(true);
        }
    }

    void EQ::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = value1 == value2;
        vmState.reg(dst(inst), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    void NEQ::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = value1 != value2;
        vmState.reg(dst(inst), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    void LT::execute(const Instruction &inst, VMState &vmState) {
        const auto &kIpt = static_cast<const VMState &>(vmState);
        const auto &value1 = kIpt.reg(reg1(inst));
        const auto &value2 = kIpt.reg(reg2(inst));
        const bool result = value1 < value2;
        vmState.reg(dst(inst), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    void LE::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = value1 <= value2;
        vmState.reg(dst(inst), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    void GT::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = value1 > value2;
        vmState.reg(dst(inst), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    void GE::execute(const Instruction &inst, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(inst));
        const auto value2 = vmState.reg(reg2(inst));
        const bool result = value1 >= value2;
        vmState.reg(dst(inst), Value{ static_cast<Integer>(result) });
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
        // call const ref is Faster
        const auto &object = vmState.reg(memberReg(inst));
        CLL_ASSERT(object.isObject(), "memberReg is not object");
        object.toObject()->call(vmState, dst(inst), argCount(inst));
    }
    void GProp::execute(const Instruction &inst, const VMState &vmState) {
        const auto &val = vmState.reg(obj(inst));
        CLL_ASSERT(val.isObject(), "gprop obj is not object");

        if(const auto *instObj = dynamic_cast<InstanceObject *>(val.toObject())) {
            Atom a{};
            if(inst.getOperand2Type() == Operand::Type::Atom)
                a = inst.getOperand2<Atom>();
            if(inst.getOperand2Type() == Operand::Type::Register) {
                const String *str = vmState.reg(inst.getOperand2<Register>()).toString();
                a = vmState.rt.atomTable.intern(str->getData(), str->length());
            }
            const auto tmp = instObj->getField(a);
            vmState.reg(dst(inst), tmp);
            return;
        }

        // maybe is static method
        // if(const auto *klass = dynamic_cast<ClassObject *>(instObj.toObject())) {
        //     if(auto *fun = klass->getMethod(name)) {
        //         vmState.reg(dst(inst), Value{ fun });
        //         return;
        //     }
        // }

        // not found return void
        vmState.reg(dst(inst), Value{});
    }

    const String *SProp::name(const Instruction &inst, const VMState &vmState) {
        if(inst.getOperand2Type() == Operand::Type::Atom)
            return vmState.rt.atomTable.get(inst.getOperand2<Atom>())->str;
        if(inst.getOperand2Type() == Operand::Type::Register)
            return vmState.reg(inst.getOperand2<Register>()).toString();
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
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "cp", dst(inst), src(inst));

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
        const auto *aEntry = vmState->rt.atomTable.get(atom(inst));
        const auto &symbol = fmt::format("\"{}\"", *aEntry->str);
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "dglobal", src(inst), symbol);

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, src(inst), vmState->reg(src(inst)));
    }
    std::string GGlobal::dump(const Instruction &inst, const VMState *vmState) {
        auto *aEntry = vmState->rt.atomTable.get(atom(inst));
        const auto &symbol = fmt::format("\"{}\"", *aEntry->str);
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "gglobal", symbol, dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, symbol, vmState->global(atom(inst)));
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

        const String *str{ nullptr };
        if(inst.getOperand2Type() == Operand::Type::Atom)
            str = vmState->rt.atomTable.get(inst.getOperand2<Atom>())->str;
        if(inst.getOperand2Type() == Operand::Type::Register) {
            str = vmState->reg(inst.getOperand2<Register>()).toString();
        }
        assert(str != nullptr);
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "gpropd", obj(inst), *str, dst(inst));
        if(!vmState)
            return insDump;
        return fmt::format("{: <30} ; {} = {}", insDump, obj(inst), vmState->reg(obj(inst)));
    }


    std::string SProp::dump(const Instruction &inst, const VMState *vmState) {
        // TODO:
        throw std::runtime_error("not implemented");
    }

    std::string GThis::dump(const Instruction &inst, const VMState *vmState) {
        const auto &symbol = fmt::format("\"{}\"", *vmState->rt.atomTable.get(atom(inst))->str);
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "gthis", symbol, dst(inst));

        if(!vmState)
            return insDump;
        Value v = vmState->getThis(atom(inst));

        return fmt::format("{: <30} ; {} = {}", insDump, symbol, v);
    }

    std::string GUpval::dump(const Instruction &inst, const VMState *vmState) {
        const auto &symbol = fmt::format("\"{}\"", *vmState->rt.atomTable.get(atom(inst))->str);
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "gupval", symbol, dst(inst));

        if(!vmState)
            return insDump;
        Value v = vmState->getUpVal(atom(inst));

        return fmt::format("{: <30} ; {} = {}", insDump, symbol, v);
    }


    std::string Ret::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {}", "ret", retReg(inst));
    }


} // namespace Cial::Bytecode::Op
