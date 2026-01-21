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
#include "types/Property.hpp"
#include "vm/Register.hpp"

namespace cial::Bytecode {

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

    void Load::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(reg(inst), vmState.curFrame()->chunk->getConstant(value(inst)).createValue(&vmState.rt));
    }

    void PushReg::execute(const Instruction &inst, const VMState &vmState) { vmState.push(vmState.reg(src(inst))); }

    void PopN::execute(const Instruction &inst, const VMState &vmState) { vmState.pop(count(inst)); }

    void CP::execute(const Instruction &inst, VMState &vmState) {
        Value srcVal = vmState.reg(src(inst));
        propObjectGet(vmState, srcVal, srcVal);
        vmState.reg(dst(inst), srcVal);
    }

    void Add::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        auto r = r1.add(r2);
        r2 = r.unwrap();
    }

    void Sub::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        auto r = r1.sub(r2);
        r2 = r.unwrap();
    }

    void Mul::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        auto r = r1.mul(r2);
        r2 = r.unwrap();
    }

    void Div::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        auto r = r1.div(r2);
        r2 = r.unwrap();
    }

    void Idiv::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        auto r = r1.idiv(r2);
        r2 = r.unwrap();
    }

    void Mod::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        auto r = r1.mod(r2);
        r2 = r.unwrap();
    }

    void Mov::execute(const Instruction &inst, VMState &vmState) {
        const Value &srcVal = vmState.reg(src(inst));
        if(propObjectSet(vmState, srcVal, vmState.regRef(dst(inst)))) {
            return;
        }
        vmState.reg(dst(inst), srcVal);
    }

    void DGlobal::execute(const Instruction &inst, VMState &vmState) {
        const Value &srcVal = vmState.regRef(src(inst));
        if(vmState.globalHas(atom(inst)) && propObjectSet(vmState, srcVal, vmState.global(atom(inst)))) {
            return;
        }

        vmState.global(atom(inst), Value{ srcVal });
    }

    void GGlobal::execute(const Instruction &inst, VMState &vmState) {
        Value srcVal = vmState.global(atom(inst));
        propObjectGet(vmState, srcVal, srcVal);
        vmState.reg(dst(inst), srcVal);
    }

    void Global::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst), Value{ vmState.context.global() });
    }

    void Super::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst),
                    vmState.global(dynamic_cast<DataObject *>(vmState.curFrame()->thisObj.asObject().unwrap())
                                       ->klass()
                                       ->meta->extends.back()));
    }

    void This::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst), Value{ vmState.curFrame()->thisObj });
    }

    void ToInt::execute(const Instruction &inst, const VMState &vmState) {
        Value &r = vmState.regRef(dst(inst));
        r = Value{ r.asInteger().unwrap() };
    }

    void ToReal::execute(const Instruction &inst, const VMState &vmState) {
        Value &r = vmState.regRef(dst(inst));
        r = Value{ r.asReal().unwrap() };
    }

    void ToString::execute(const Instruction &inst, const VMState &vmState) {
        Value &r = vmState.regRef(dst(inst));
        Value s = r;
        assert(s.toString().isOk());
        r = s;
    }

    void ChgThis::execute(const Instruction &inst, const VMState &vmState) {
        const auto &srcVal = vmState.reg(src(inst));
        const auto &dstVal = vmState.reg(dst(inst));
        dynamic_cast<Function *>(dstVal.asObject().unwrap())->thisObj = srcVal.asObject().unwrap();
    }

    void Inv::execute(const Instruction &inst, const VMState &vmState) {
        const auto &dstVal = vmState.reg(dst(inst));
        dynamic_cast<DataObject *>(dstVal.asObject().unwrap())->invalidate();
    }

    void ChkInv::execute(const Instruction &inst, const VMState &vmState) {
        const auto &srcVal = vmState.reg(src(inst));
        Value &dstVal = vmState.regRef(dst(inst));
        dstVal = Value{ dynamic_cast<DataObject *>(srcVal.asObject().unwrap())->isValid() };
    }

    void ChkIns::execute(const Instruction &inst, const VMState &vmState) {
        Value &dstVal = vmState.regRef(dst(inst));
        if(const Value &srcVal = vmState.reg(src(inst)); srcVal.isString()) {
            const Atom atom = vmState.context.rt().atomTable.intern(*srcVal.asString().value());
            dstVal = Value{ dstVal.asObject().unwrap()->instanceOf(atom) };
            return;
        }
        dstVal = Value{ false };
    }

    void Test::execute(const Instruction &inst, VMState &vmState) {
        if(vmState.reg(reg(inst)).asBool()) {
            vmState.setZF(true);
        }
    }

    void EQ::execute(const Instruction &inst, VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.equals(r2);
        r2 = Value{ r };
        vmState.setZF(r);
    }

    void NEQ::execute(const Instruction &inst, VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = !r1.equals(r2);
        r2 = Value{ r };
        vmState.setZF(r);
    }

    void AbsEQ::execute(const Instruction &inst, VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.discernEquals(r2);
        r2 = Value{ r };
        vmState.setZF(r);
    }

    void AbsNEQ::execute(const Instruction &inst, VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = !r1.discernEquals(r2);
        r2 = Value{ r };
        vmState.setZF(r);
    }

    void LT::execute(const Instruction &inst, VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.littlerThan(r2).unwrap();
        r2 = Value{ r };
        vmState.setZF(r);
    }

    void LE::execute(const Instruction &inst, VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = !r1.greaterThan(r2).unwrap();
        r2 = Value{ r };
        vmState.setZF(r);
    }

    void GT::execute(const Instruction &inst, VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.greaterThan(r2).unwrap();
        r2 = Value{ r };
        vmState.setZF(r);
    }

    void GE::execute(const Instruction &inst, VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = !r1.littlerThan(r2).unwrap();
        r2 = Value{ r };
        vmState.setZF(r);
    }

    void LAnd::execute(const Instruction &inst, VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.logicalAnd(r2);
        r2 = Value{ r };
        vmState.setZF(r);
    }

    void LOr::execute(const Instruction &inst, VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.logicalOr(r2);
        r2 = Value{ r };
        vmState.setZF(r);
    }

    void BXor::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.bitwiseXor(r2).unwrap();
    }

    void BOr::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.bitwiseOr(r2).unwrap();
    }

    void BAnd::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.bitwiseAnd(r2).unwrap();
    }

    void BLShift::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.bitwiseLeftShift(r2).unwrap();
    }

    void BRShift::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.bitwiseRightShift(r2).unwrap();
    }

    void BURShift::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.bitwiseUnsignedRightShift(r2).unwrap();
    }

    void Jmp::execute(const Instruction &inst, const VMState &vmState) { vmState.setPC(label(inst)); }

    void JmpE::execute(const Instruction &inst, const VMState &vmState) {
        if(vmState.getZF()) {
            vmState.setPC(label(inst));
        }
    }

    void JmpNE::execute(const Instruction &inst, const VMState &vmState) {
        if(!vmState.getZF()) {
            vmState.setPC(label(inst));
        }
    }

    void Call::execute(const Instruction &inst, VMState &vmState) {
        const auto &object = vmState.reg(memberReg(inst)).asObject().unwrap();
        object->call(vmState, dst(inst), argCount(inst));
    }

    void GProp::execute(const Instruction &inst, VMState &vmState) {
        const auto &val = vmState.reg(obj(inst));
        const String &name = *vmState.reg(memberReg(inst)).asString().unwrap();
        const Atom atom = vmState.rt.atomTable.intern(name);

        if(val.isObject()) {
            auto *obj = val.asObject().value();

            if(auto *global = dynamic_cast<GlobalObject *>(obj)) {
                // for global, proxy object, maybe get extends object in dataObject?
                if(global->proxy) {
                    // TODO:
                    // global->proxy->getSuperDataClass(atom);
                    // return;
                    throw std::runtime_error("Not implemented");
                }
                vmState.reg(dst(inst), global->getProp(atom));
                return;
            }

            if(const auto *classObject = dynamic_cast<ClassObject *>(obj)) {
                // TODO:
                auto tmp = obj->getProp(atom);
                propObjectGet(vmState, tmp, tmp);
                if(tmp.isObject()) {
                    auto *fun = dynamic_cast<Function *>(tmp.asObject().value());
                    fun->thisObj = vmState.curFrame()->thisObj.asObject().value();
                }
                vmState.reg(dst(inst), tmp);
                return;
            }

            auto tmp = obj->getProp(atom);
            propObjectGet(vmState, tmp, tmp);
            vmState.reg(dst(inst), tmp);
            return;
        }

        // Native method
        const TypeId tId = ValueToTypeId::getId(val);
        assert(tId != TypeId::None);
        NativeFunction *nativeFn = vmState.context.findMethod(tId, atom);
        assert(nativeFn != nullptr);
        nativeFn = vmState.rt.create<NativeFunction>(*nativeFn).get();
        nativeFn->setThisObj(val);
        vmState.reg(dst(inst), Value{ nativeFn });
    }

    const String *DProp::name(const Instruction &inst, const VMState &vmState) {
        if(inst.getOperand2Type() == Operand::Type::Atom)
            return vmState.rt.atomTable.get(inst.getOperand2<Atom>())->str;
        if(inst.getOperand2Type() == Operand::Type::Register)
            return vmState.reg(inst.getOperand2<Register>()).asString().unwrap();
        CLL_ASSERT(false, "unknown inst dprop operand2 type");
        return {};
    }

    Value DProp::value(const Instruction &inst, const VMState &vmState) {
        if(inst.getOperand3Type() == Operand::Type::ConstIndex)
            return vmState.curFrame()->chunk->getConstant(inst.getOperand3<ConstIdx>()).createValue(&vmState.rt);
        if(inst.getOperand3Type() == Operand::Type::Register)
            return vmState.reg(inst.getOperand3<Register>());
        CLL_ASSERT(false, "unknown inst dprop operand3 type");
        return {};
    }

    void DProp::execute(const Instruction &inst, VMState &vmState) {
        // TODO:
        // const auto &instObj = vmState.reg(obj(inst));
        // CLL_ASSERT(instObj.isObject(), "gprop obj is not object");
        //
        // if(auto *inst = dynamic_cast<ClassObject *>(instObj.toObject())) {
        //     inst->set(name(inst, vmState), value(inst, vmState));
        // }
        throw std::runtime_error("not implemented");
    }

    void GThis::execute(const Instruction &inst, VMState &vmState) {
        Value srcVal = vmState.getThis(atom(inst));
        propObjectGet(vmState, srcVal, srcVal);
        vmState.reg(dst(inst), srcVal);
    }

    void DThis::execute(const Instruction &inst, VMState &vmState) {
        const Value &srcVal = vmState.regRef(src(inst));

        if(vmState.hasThis(atom(inst))) {
            if(propObjectSet(vmState, srcVal, vmState.getThis(atom(inst)))) {
                return;
            }
        }

        vmState.setThis(atom(inst), srcVal);
    }

    void GUpval::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst), vmState.getUpVal(atom(inst)));
    }

    void LNot::execute(const Instruction &inst, const VMState &vmState) {
        const Register srcReg = src(inst);
        vmState.regRef(srcReg).toLogicalNot();
    }

    void ChgSign::execute(const Instruction &inst, const VMState &vmState) {
        auto r = vmState.regRef(src(inst)).toSignChange();
        if(r.isFailed())
            throw r.getErr();
    }

    void Ret::execute(const Instruction &inst, VMState &vmState) {
        const auto &value = vmState.reg(retReg(inst));
        const auto frame = vmState.curFrame();
        CLL_ASSERT(frame->ret, "frame.ret val is empty");
        vmState.prevFrame()->getReg(*frame->ret) = value;
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
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "add", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string Sub::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "sub", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }
    std::string Mul::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "mul", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string Div::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "div", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string Idiv::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "idiv", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string Mod::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "mod", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
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

    std::string Global::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "global", dst(inst));
    }

    std::string Super::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "super", dst(inst));
    }

    std::string This::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "this", dst(inst));
    }

    std::string ToInt::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "int", dst(inst));
    }

    std::string ToReal::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "real", dst(inst));
    }

    std::string ToString::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "string", dst(inst));
    }

    std::string ChgThis::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4}", "chgthis", dst(inst), src(inst));
    }

    std::string Inv::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "inv", dst(inst));
    }

    std::string ChkInv::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4}", "chkinv", dst(inst), src(inst));
    }

    std::string ChkIns::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4} {: <4}", "chkins", dst(inst), src(inst));
    }

    std::string Test::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "test", reg(inst));
    }

    std::string EQ::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "eq", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string NEQ::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "neq", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string LT::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "lt", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string LE::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "le", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string AbsEQ::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "abseq", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string AbsNEQ::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "absneq", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string GT::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "gt", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string GE::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "ge", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string LAnd::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "land", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string LOr::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "lor", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string BXor::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "bxor", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string BOr::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "bor", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string BAnd::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "band", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string BLShift::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "blshift", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string BRShift::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "brshift", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
    }

    std::string BURShift::dump(const Instruction &inst, const VMState *vmState) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "burshift", src(inst), dst(inst));

        if(!vmState)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, src(inst), vmState->reg(src(inst)), dst(inst),
                           vmState->reg(dst(inst)));
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

    std::string DProp::dump(const Instruction &inst, const VMState *vmState) {
        // TODO:
        throw std::runtime_error("not implemented");
    }

    std::string GThis::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} atom_{} {: <4}", "gthis", atom(inst).v, dst(inst));
    }

    std::string DThis::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} atom_{} {: <4}", "dthis", atom(inst).v, src(inst));
    }

    std::string GUpval::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} atom_{} {: <4}", "gupval", atom(inst).v, dst(inst));
    }

    std::string LNot::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "lnot", src(inst));
    }

    std::string ChgSign::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {: <4}", "chgsign", src(inst));
    }

    std::string Ret::dump(const Instruction &inst, const VMState *vmState) {
        return fmt::format("{: <10} {}", "ret", retReg(inst));
    }

} // namespace cial::Bytecode
