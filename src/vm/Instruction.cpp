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

#include "VMState.hpp"
#include "logging/Logger.hpp"
#include "types/Class.hpp"
#include "types/Function.hpp"
#include "types/Object.hpp"
#include "vm/Register.hpp"

namespace Cial::Bytecode::Op {

    void Load::execute(const Instruction &itt, const VMState &vmState) {
        vmState.reg(reg(itt), vmState.curFrame()->chunk->getConstant(value(itt)).createValue(vmState.rt()));
    }

    std::string Load::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4} {: <4}", "load", reg(itt), value(itt));
    }

    void PushReg::execute(const Instruction &itt, VMState &vmState) { vmState.push(vmState.reg(src(itt))); }

    std::string PushReg::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4}", "pushreg", src(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, src(itt), vmState.reg(src(itt)));
    }

    void PopN::execute(const Instruction &itt, VMState &vmState) { vmState.pop(count(itt)); }

    std::string PopN::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4}", "popn", count(itt));
    }

    void CP::execute(const Instruction &itt, const VMState &vmState) { vmState.reg(dst(itt), vmState.reg(src(itt))); }

    std::string CP::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "cp", dst(itt), src(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, src(itt), vmState.reg(src(itt)));
    }

    void Add::execute(const Instruction &itt, const VMState &vmState) {
        vmState.reg(dst(itt), vmState.reg(reg1(itt)) + vmState.reg(reg2(itt)));
    }

    std::string Add::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "add", reg1(itt), reg2(itt), dst(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, reg1(itt), vmState.reg(reg1(itt)), reg2(itt),
                           vmState.reg(reg2(itt)));
    }

    void Sub::execute(const Instruction &itt, const VMState &vmState) {
        vmState.reg(dst(itt), vmState.reg(reg1(itt)) - vmState.reg(reg2(itt)));
    }

    std::string Sub::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "sub", reg1(itt), reg2(itt), dst(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, reg1, vmState.reg(reg1(itt)), reg2(itt),
                           vmState.reg(reg2(itt)));
    }

    void Mul::execute(const Instruction &itt, const VMState &vmState) {
        vmState.reg(dst(itt), vmState.reg(reg1(itt)) * vmState.reg(reg2(itt)));
    }

    std::string Mul::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "mul", reg1(itt), reg2(itt), dst(itt));
    }

    void Div::execute(const Instruction &itt, const VMState &vmState) {
        vmState.reg(dst(itt), vmState.reg(reg1(itt)) / vmState.reg(reg2(itt)));
    }

    std::string Div::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "div", reg1(itt), reg2(itt), dst(itt));
    }

    void Mov::execute(const Instruction &itt, const VMState &vmState) {
        const Value &srcVal = vmState.reg(src(itt));
        vmState.reg(dst(itt), srcVal);
    }

    std::string Mov::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4} {: <4}", "mov", src(itt), dst(itt));
    }

    void DGlobal::execute(const Instruction &itt, const VMState &vmState) {
        const auto &value = vmState.reg(src(itt));
        vmState.global(atom(itt), Value{ value });
    }

    std::string DGlobal::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        auto *aEntry = vmState.rt().atomTable.get(atom(itt));
        const auto &symbol = fmt::format("\"{}\"", aEntry->str);
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "dglobal", src(itt), symbol);

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, src(itt), vmState.reg(src(itt)));
    }

    void GGlobal::execute(const Instruction &itt, const VMState &vmState) {
        const auto &value = vmState.global(atom(itt));
        vmState.reg(dst(itt), value);
    }

    std::string GGlobal::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        auto *aEntry = vmState.rt().atomTable.get(atom(itt));
        const auto &symbol = fmt::format("\"{}\"", aEntry->str);
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "gglobal", symbol, dst(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, symbol, vmState.global(atom(itt)));
    }

    void Test::execute(const Instruction &itt, VMState &vmState) {
        if(vmState.reg(reg(itt)).toBool()) {
            vmState.setZF(true);
        }
    }

    std::string Test::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4}", "test", reg(itt));
    }

    void EQ::execute(const Instruction &itt, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(itt));
        const auto value2 = vmState.reg(reg2(itt));
        const bool result = value1 == value2;
        vmState.reg(dst(itt), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    std::string EQ::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "eq", reg1(itt), reg2(itt), dst(itt));
    }

    void NEQ::execute(const Instruction &itt, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(itt));
        const auto value2 = vmState.reg(reg2(itt));
        const bool result = value1 != value2;
        vmState.reg(dst(itt), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    std::string NEQ::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4}, {: <4}, {: <4}", "neq", reg1(itt), reg2(itt), dst(itt));
    }

    void LT::execute(const Instruction &itt, VMState &vmState) {
        const auto &kIpt = static_cast<const VMState &>(vmState);
        const auto &value1 = kIpt.reg(reg1(itt));
        const auto &value2 = kIpt.reg(reg2(itt));
        const bool result = value1 < value2;
        vmState.reg(dst(itt), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    std::string LT::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "lt", reg1(itt), reg2(itt), dst(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, reg1(itt), vmState.reg(reg1(itt)), reg2(itt),
                           vmState.reg(reg2(itt)));
    }

    void LE::execute(const Instruction &itt, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(itt));
        const auto value2 = vmState.reg(reg2(itt));
        const bool result = value1 <= value2;
        vmState.reg(dst(itt), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    std::string LE::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "le", reg1(itt), reg2(itt), dst(itt));
    }

    void GT::execute(const Instruction &itt, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(itt));
        const auto value2 = vmState.reg(reg2(itt));
        const bool result = value1 > value2;
        vmState.reg(dst(itt), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    std::string GT::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "ge", reg1(itt), reg2(itt), dst(itt));
    }

    void GE::execute(const Instruction &itt, VMState &vmState) {
        const auto value1 = vmState.reg(reg1(itt));
        const auto value2 = vmState.reg(reg2(itt));
        const bool result = value1 >= value2;
        vmState.reg(dst(itt), Value{ static_cast<Integer>(result) });
        vmState.setZF(result);
    }

    std::string GE::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "ge", reg1(itt), reg2(itt), dst(itt));
    }

    void AbsEQ::execute(const Instruction &, VMState &) {
        // TODO:
        assert(false);
    }

    std::string AbsEQ::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "abseq", reg1(itt), reg2(itt), dst(itt));
    }

    void Jmp::execute(const Instruction &itt, VMState &vmState) { vmState.setPC(label(itt)); }

    std::string Jmp::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {: <4}", "jmp", label(itt));
    }

    void JmpE::execute(const Instruction &itt, VMState &vmState) {
        if(vmState.getZF()) {
            vmState.setPC(label(itt));
        }
    }

    std::string JmpE::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4}", "jmpe", label(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; ZF = {}", insDump, vmState.getZF());
    }

    void JmpNE::execute(const Instruction &itt, VMState &vmState) {
        if(!vmState.getZF()) {
            vmState.setPC(label(itt));
        }
    }

    std::string JmpNE::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4}", "jmpne", label(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; ZF = {}", insDump, vmState.getZF());
    }

    void Call::execute(const Instruction &itt, VMState &vmState) {
        // call const ref is Faster
        const auto &object = vmState.reg(memberReg(itt));
        CLL_ASSERT(object.isObject(), "memberReg is not object");
        object.toObject()->call(vmState, dst(itt), argCount(itt));
    }

    std::string Call::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "call", memberReg(itt), dst(itt), argCount(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, memberReg(itt), vmState.reg(memberReg(itt)));
    }

    void GProp::execute(const Instruction &itt, const VMState &vmState) {
        const auto &instObj = vmState.reg(obj(itt));
        CLL_ASSERT(instObj.isObject(), "gprop obj is not object");

        if(const auto *inst = dynamic_cast<InstanceObject *>(instObj.toObject())) {
            Atom a{};
            if(itt.getOperand2Type() == Operand::Type::Atom)
                a = itt.getOperand2<Atom>();
            if(itt.getOperand2Type() == Operand::Type::Register) {
                const String *str = vmState.reg(itt.getOperand2<Register>()).toString();
                a = vmState.rt().atomTable.intern(str->getData(), str->length());
            }
            const auto tmp = inst->getField(a);
            // if(itt.getOperand2Type() == Operand::Type::Register) {
            // vmState.rt().atomTable.release(a);
            // }
            vmState.reg(dst(itt), tmp);
            return;
        }

        // maybe is static method
        // if(const auto *klass = dynamic_cast<ClassObject *>(instObj.toObject())) {
        //     if(auto *fun = klass->getMethod(name)) {
        //         vmState.reg(dst(itt), Value{ fun });
        //         return;
        //     }
        // }

        // not found return void
        vmState.reg(dst(itt), Value{});
    }


    std::string GProp::dump(const Instruction &itt, const VMState &vmState, const bool info) {

        const char *str;
        if(itt.getOperand2Type() == Operand::Type::Atom)
            str = vmState.rt().atomTable.get(itt.getOperand2<Atom>())->str;
        if(itt.getOperand2Type() == Operand::Type::Register) {
            str = vmState.reg(itt.getOperand2<Register>()).toString()->getData();
        }
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "gpropd", obj(itt), str, dst(itt));
        if(!info)
            return insDump;
        return fmt::format("{: <30} ; {} = {}", insDump, obj(itt), vmState.reg(obj(itt)));
    }


    const char *SProp::name(const Instruction &itt, const VMState &vmState) {
        if(itt.getOperand2Type() == Operand::Type::Atom)
            return vmState.rt().atomTable.get(itt.getOperand2<Atom>())->str;
        if(itt.getOperand2Type() == Operand::Type::Register)
            return vmState.reg(itt.getOperand2<Register>()).toString()->getData();
        CLL_ASSERT(false, "unknown inst sprop operand2 type");
    }

    Value SProp::value(const Instruction &itt, const VMState &vmState) {
        if(itt.getOperand3Type() == Operand::Type::ConstIndex)
            return vmState.curFrame()->chunk->getConstant(itt.getOperand3<ConstIdx>()).createValue(vmState.rt());
        if(itt.getOperand3Type() == Operand::Type::Register)
            return vmState.reg(itt.getOperand3<Register>());
        CLL_ASSERT(false, "unknown inst sprop operand3 type");
    }

    void SProp::execute(const Instruction &itt, VMState &vmState) {
        // TODO:
        // const auto &instObj = vmState.reg(obj(itt));
        // CLL_ASSERT(instObj.isObject(), "gprop obj is not object");
        //
        // if(auto *inst = dynamic_cast<ClassObject *>(instObj.toObject())) {
        //     inst->set(name(itt, vmState), value(itt, vmState));
        // }
        throw std::runtime_error("not implemented");
    }

    std::string SProp::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        // TODO:
        throw std::runtime_error("not implemented");
    }

    void GUpval::execute(const Instruction &itt, const VMState &vmState) {
        vmState.reg(dst(itt), vmState.getUpVal(atom(itt)));
    }

    std::string GUpval::dump(const Instruction &itt, const VMState &vmState, const bool info) {
        const auto &symbol = fmt::format("\"{}\"", vmState.rt().atomTable.get(atom(itt))->str);
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "gupval", symbol, dst(itt));

        if(!info)
            return insDump;
        Value v{};
        if(const auto &callFrame = vmState.curFrame(); callFrame->thisValue.isObject()) {
            if(const auto *instanceObject = dynamic_cast<InstanceObject *>(callFrame->thisValue.toObject())) {
                v = instanceObject->getField(atom(itt));
            }
        }

        if(v.isVoid()) {
            v = vmState.global(atom(itt));
        }

        return fmt::format("{: <30} ; {} = {}", insDump, symbol, v);
    }

    void Ret::execute(const Instruction &itt, VMState &vmState) {
        const auto &value = vmState.reg(retReg(itt));
        const auto frame = vmState.curFrame();
        CLL_ASSERT(frame->ret, "frame.ret val is empty");
        vmState.prev()->getReg(*frame->ret) = value;
        vmState.freeCallFrame();
    }

    std::string Ret::dump(const Instruction &itt, const VMState &, bool) {
        return fmt::format("{: <10} {}", "ret", retReg(itt));
    }

    void Instruction::execute(const Instruction &itt, VMState &vmState) {
#define HANDLE_OPCODE(OP)                                                                                              \
    case OpCode::OP:                                                                                                   \
        OP::execute(itt, vmState);                                                                                     \
        break;
        switch(itt.opcode) {
            OPCODE_ENUMS(HANDLE_OPCODE)
            default:;
        }
    }

    std::string Instruction::dump(const Instruction &itt, const VMState &vmState, const bool info) {
#define DUMP_OPCODE(OP)                                                                                                \
    case OpCode::OP:                                                                                                   \
        return OP::dump(itt, vmState, info);
        switch(itt.opcode) {
            OPCODE_ENUMS(DUMP_OPCODE)
            default:;
        }
        return "";
    }


} // namespace Cial::Bytecode::Op
