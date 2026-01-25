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
#include <fmt/ranges.h>

#include "VMDebug.hpp"
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
        switch(inst._opcode) {
            OPCODE_ENUMS(HANDLE_OPCODE)
            default:;
        }
    }

    std::string Instruction::dump(const Instruction &inst, const VMState *vmState) {
#define DUMP_OPCODE(OP)                                                                                                \
    case OpCode::OP:                                                                                                   \
        return OP::dump(inst, vmState);
        switch(inst._opcode) {
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

    void NOP::execute(const Instruction &, const VMState &) {}

    void Load::execute(const Instruction &inst, const VMState &vmState) {
        vmState.regRef(dst(inst)) = vmState.curFrame()->chunk->getConstant(value(inst)).createValue(&vmState.rt);
    }

    void ILoad::execute(const Instruction &inst, const VMState &vmState) {
        vmState.regRef(dst(inst)) = Value{ value(inst) };
    }

    void Push::execute(const Instruction &inst, const VMState &vmState) { vmState.push(vmState.reg(src(inst))); }

    void PopN::execute(const Instruction &inst, const VMState &vmState) { vmState.pop(count(inst)); }

    void CP::execute(const Instruction &inst, VMState &vmState) {
        Value srcVal = vmState.reg(src(inst));
        propObjectGet(vmState, srcVal, srcVal);
        vmState.reg(dst(inst), srcVal);
    }

    void Add::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.add(r2).unwrap();
    }

    void IAdd::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1{ src(inst) };
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.add(r2).unwrap();
    }

    void Sub::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.sub(r2).unwrap();
    }

    void ISub::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1{ src(inst) };
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.sub(r2).unwrap();
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
        vmState.reg(
            dst(inst),
            Value{ dynamic_cast<DataObject *>(vmState.curFrame()->thisObj.asObject().unwrap())->getSuperClass() });
    }

    void This::execute(const Instruction &inst, const VMState &vmState) {
        vmState.reg(dst(inst), Value{ vmState.curFrame()->thisObj });
    }

    void ToInt::execute(const Instruction &inst, const VMState &vmState) {
        const auto r = vmState.regRef(dst(inst)).toInteger();
        if(r.isFailed()) {
            throw r.getErr();
        }
    }

    void ToReal::execute(const Instruction &inst, const VMState &vmState) {
        const auto r = vmState.regRef(dst(inst)).toReal();
        if(r.isFailed()) {
            throw r.getErr();
        }
    }

    void ToString::execute(const Instruction &inst, const VMState &vmState) {
        const auto r = vmState.regRef(dst(inst)).toString();
        if(r.isFailed()) {
            throw r.getErr();
        }
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

    void Test::execute(const Instruction &inst, VMState &vmState) { vmState.setZF(vmState.reg(reg(inst)).asBool()); }

    void EQ::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.equals(r2);
        r2 = Value{ r };
    }

    void NEQ::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = !r1.equals(r2);
        r2 = Value{ r };
    }

    void AbsEQ::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.discernEquals(r2);
        r2 = Value{ r };
    }

    void AbsNEQ::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = !r1.discernEquals(r2);
        r2 = Value{ r };
    }

    void LT::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.littlerThan(r2).unwrap();
        r2 = Value{ r };
    }

    void LE::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = !r1.greaterThan(r2).unwrap();
        r2 = Value{ r };
    }

    void GT::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.greaterThan(r2).unwrap();
        r2 = Value{ r };
    }

    void GE::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = !r1.littlerThan(r2).unwrap();
        r2 = Value{ r };
    }

    void LAnd::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.logicalAnd(r2);
        r2 = Value{ r };
    }

    void LOr::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        const bool r = r1.logicalOr(r2);
        r2 = Value{ r };
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

    void BlShift::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.bitwiseLeftShift(r2).unwrap();
    }

    void BrShift::execute(const Instruction &inst, const VMState &vmState) {
        const Value r1 = vmState.reg(src(inst));
        Value &r2 = vmState.regRef(dst(inst));
        r2 = r1.bitwiseRightShift(r2).unwrap();
    }

    void BurShift::execute(const Instruction &inst, const VMState &vmState) {
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

    void DProp::execute(const Instruction &inst, const VMState &vmState) {
        const auto &r = vmState.reg(obj(inst));
        CLL_ASSERT(r.isObject(), "gprop obj is not object");
        const String &name = *vmState.reg(memberReg(inst)).asString().unwrap();
        const Atom atom = vmState.rt.atomTable.intern(name);
        r.asObject().unwrap()->setProp(atom, vmState.reg(src(inst)));
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
        const Register srcReg = dst(inst);
        vmState.regRef(srcReg).toLogicalNot();
    }

    void ChgSign::execute(const Instruction &inst, const VMState &vmState) {
        auto r = vmState.regRef(dst(inst)).toSignChange();
        if(r.isFailed())
            throw r.getErr();
    }

    void Throw::execute(const Instruction &inst, VMState &vmState) { vmState.throwException(vmState.reg(src(inst))); }

    void Debugger::execute(const Instruction &inst, const VMState &vmState) {
        std::string regs = vmState.dumpCurRegisters();
        std::string localVarInfo = vmState.dumpCurLocalVars();
        std::string chunk = vmState.dumpCurInstructions();
        std::string constants = vmState.dumpCurConstants();

        fmt::println("{}", regs);
        fmt::println("====================");
        fmt::println("{}", localVarInfo);
        fmt::println("====================");
        fmt::println("{}", constants);
        fmt::println("====================");
        fmt::println("{}", chunk);
        DEBUG_BREAK();
    }

    void Ret::execute(const Instruction &inst, VMState &vmState) {
        const auto &value = vmState.reg(retReg(inst));
        const auto frame = vmState.curFrame();
        CLL_ASSERT(frame->ret, "frame.ret val is empty");
        vmState.prevFrame()->getReg(*frame->ret) = value;
        vmState.freeCallFrame();
    }

    String DumpInst::dumpOperand(const Operand &operand) {
        switch(operand.type()) {
            case Operand::Type::Register:
                return String{ fmt::format("{: <4}", operand.value<Register>()) };
            case Operand::Type::Label:
                return String{ fmt::format("{: <4}", operand.value<Label>()) };
            case Operand::Type::ConstIndex:
                return String{ fmt::format("{: <4}", operand.value<ConstIdx>()) };
            case Operand::Type::Number:
                return String{ fmt::format("{: <4}", operand.value<Integer>()) };
            case Operand::Type::Atom:
                return String{ fmt::format("atom_{}", operand.value<Atom>().v) };
            default:
                return ""_str;
        }
    }

    namespace {
        struct DumpComment {
            std::vector<std::string> items;

            void add(std::string s) { items.push_back(std::move(s)); }

            [[nodiscard]] bool empty() const { return items.empty(); }

            [[nodiscard]] std::string format() const { return fmt::format("; {}", fmt::join(items, ", ")); }
        };

        std::string dumpWithComment(std::string insDump, const DumpComment &c) {
            if(c.empty())
                return insDump;

            return fmt::format("{: <30} {}", insDump, c.format());
        }

        void dumpOperandComment(DumpComment &c, const Operand &operand, const VMState *vm) {
            if(!vm)
                return;

            switch(operand.type()) {
                case Operand::Type::Register: {
                    auto r = operand.value<Register>();
                    c.add(fmt::format("{} = {}", r, vm->reg(r)));
                    break;
                }
                case Operand::Type::ConstIndex: {
                    auto idx = operand.value<ConstIdx>();
                    auto s = vm->curFrame()->chunk->dumpConstant(&vm->context.rt(), idx);
                    c.add(fmt::format("{} = {}", idx, s));
                    break;
                }
                case Operand::Type::Atom: {
                    auto a = operand.value<Atom>();
                    if(const auto *e = vm->rt.atomTable.get(a))
                        c.add(fmt::format("atom_{}=\"{}\"", a.v, *e->str));
                    break;
                }
                default:
                    break;
            }
        }
    } // namespace

    std::string DumpInst::autoDump(std::string_view name, const Instruction &inst, const VMState *vm) {
        DumpComment c;
        std::string out = fmt::format("{: <10}", name);
        auto d = [&out, &c, &vm](const Operand &operand) {
            out += " ";
            out += dumpOperand(operand).toStdStr();
            dumpOperandComment(c, operand, vm);
        };

        if(inst._operand1)
            d(inst._operand1.value());
        if(inst._operand2)
            d(inst._operand2.value());
        if(inst._operand3)
            d(inst._operand3.value());

        return dumpWithComment(out, c);
    }

#define DEF_AUTO_DUMP(className, instName)                                                                             \
    std::string className::dump(const Instruction &inst, const VMState *vm) {                                          \
        return DumpInst::autoDump(instName, inst, vm);                                                                 \
    }

    DEF_AUTO_DUMP(NOP, "nop");

    // 基础加载与推栈
    DEF_AUTO_DUMP(Load, "load");
    DEF_AUTO_DUMP(ILoad, "iload");
    DEF_AUTO_DUMP(Push, "push");
    DEF_AUTO_DUMP(PopN, "pop_n");
    DEF_AUTO_DUMP(Mov, "mov");

    // 算术运算 (带状态输出)
    DEF_AUTO_DUMP(CP, "cp");
    DEF_AUTO_DUMP(Add, "add");
    DEF_AUTO_DUMP(IAdd, "iadd");
    DEF_AUTO_DUMP(Sub, "sub");
    DEF_AUTO_DUMP(ISub, "isub");
    DEF_AUTO_DUMP(Mul, "mul");
    DEF_AUTO_DUMP(Div, "div");
    DEF_AUTO_DUMP(Idiv, "idiv");
    DEF_AUTO_DUMP(Mod, "mod");

    // 全局与作用域
    DEF_AUTO_DUMP(DGlobal, "d_global");
    DEF_AUTO_DUMP(GGlobal, "g_global");
    DEF_AUTO_DUMP(Global, "global");
    DEF_AUTO_DUMP(GUpval, "g_upval");

    // 对象与上下文
    DEF_AUTO_DUMP(Super, "super");
    DEF_AUTO_DUMP(This, "this");
    DEF_AUTO_DUMP(ChgThis, "chg_this");
    DEF_AUTO_DUMP(GThis, "g_this");
    DEF_AUTO_DUMP(DThis, "d_this");

    // 类型转换
    DEF_AUTO_DUMP(ToInt, "int");
    DEF_AUTO_DUMP(ToReal, "real");
    DEF_AUTO_DUMP(ToString, "string");

    // 逻辑与比较
    DEF_AUTO_DUMP(Inv, "inv");
    DEF_AUTO_DUMP(ChkInv, "chk_inv");
    DEF_AUTO_DUMP(ChkIns, "chk_ins");
    DEF_AUTO_DUMP(Test, "test");
    DEF_AUTO_DUMP(EQ, "eq");
    DEF_AUTO_DUMP(NEQ, "neq");
    DEF_AUTO_DUMP(LT, "lt");
    DEF_AUTO_DUMP(LE, "le");
    DEF_AUTO_DUMP(GT, "gt");
    DEF_AUTO_DUMP(GE, "ge");
    DEF_AUTO_DUMP(AbsEQ, "abs_eq");
    DEF_AUTO_DUMP(AbsNEQ, "abs_neq");

    // 位运算与逻辑运算
    DEF_AUTO_DUMP(LAnd, "land");
    DEF_AUTO_DUMP(LOr, "l_or");
    DEF_AUTO_DUMP(LNot, "l_not");
    DEF_AUTO_DUMP(BXor, "b_xor");
    DEF_AUTO_DUMP(BOr, "b_or");
    DEF_AUTO_DUMP(BAnd, "b_and");
    DEF_AUTO_DUMP(BlShift, "bl_shift");
    DEF_AUTO_DUMP(BrShift, "br_shift");
    DEF_AUTO_DUMP(BurShift, "bur_shift");

    // 控制流
    DEF_AUTO_DUMP(Jmp, "jmp");
    DEF_AUTO_DUMP(JmpE, "jmp_e");
    DEF_AUTO_DUMP(JmpNE, "jmp_ne");
    DEF_AUTO_DUMP(Call, "call");
    DEF_AUTO_DUMP(Ret, "ret");

    // 属性操作
    DEF_AUTO_DUMP(GProp, "g_prop");
    DEF_AUTO_DUMP(DProp, "d_prop");

    // 其他
    DEF_AUTO_DUMP(ChgSign, "chg_sign");
    DEF_AUTO_DUMP(Throw, "throw");
    DEF_AUTO_DUMP(Debugger, "debugger");

} // namespace cial::Bytecode
