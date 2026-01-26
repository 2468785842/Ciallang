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
#include "gen/Instruction.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "logging/Logger.hpp"
#include "types/Class.hpp"
#include "types/Function.hpp"
#include "types/Object.hpp"
#include "types/Property.hpp"
#include "vm/VMDebug.hpp"
#include "vm/VMState.hpp"

namespace cial::Inter {
    using namespace cial::Bytecode;

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

    void NOP::execute(VMState &vmState) {}

    void Debugger::execute(VMState &vmState) {
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

    void Push::execute(VMState &vmState) { vmState.push(vmState.reg(src().index())); }

    void PopN::execute(VMState &vmState) { vmState.pop(count()); }

    void Global::execute(VMState &vmState) { vmState.regRef(dst().index()) = Value{ vmState.context.global() }; }

    void Super::execute(VMState &vmState) {
        vmState.regRef(dst().index()) =
            Value{ dynamic_cast<DataObject *>(vmState.curFrame()->thisObj.asObject().unwrap())->getSuperClass() };
    }

    void This::execute(VMState &vmState) { vmState.regRef(dst().index()) = Value{ vmState.curFrame()->thisObj }; }

    void Test::execute(VMState &vmState) { vmState.setZF(vmState.reg(src().index()).asBool()); }

    void Inv::execute(VMState &vmState) {
        const auto &dstVal = vmState.reg(src().index());
        dynamic_cast<DataObject *>(dstVal.asObject().unwrap())->invalidate();
    }

    void Throw::execute(VMState &vmState) { vmState.throwException(vmState.reg(src().index())); }

    void Ret::execute(VMState &vmState) {
        const auto &value = vmState.reg(src().index());
        const auto frame = vmState.curFrame();
        CLL_ASSERT(frame->ret, "frame.ret val is empty");
        vmState.prevFrame()->getReg(*frame->ret) = value;
        vmState.freeCallFrame();
    }

    void Jmp::execute(VMState &vmState) { vmState.setPC(label().address()); }

    void JmpE::execute(VMState &vmState) {
        if(vmState.getZF()) {
            vmState.setPC(label().address());
        }
    }

    void JmpNE::execute(VMState &vmState) {
        if(!vmState.getZF()) {
            vmState.setPC(label().address());
        }
    }

    void ToInt::execute(VMState &vmState) {
        const Value srcReg = vmState.reg(src().index());
        Value &dstReg = vmState.regRef(dst().index());
        dstReg = srcReg;
        const auto r = dstReg.toInteger();
        if(r.isFailed()) {
            throw r.getErr();
        }
    }

    void ToReal::execute(VMState &vmState) {
        const Value srcReg = vmState.reg(src().index());
        Value &dstReg = vmState.regRef(dst().index());
        dstReg = srcReg;
        const auto r = dstReg.toReal();
        if(r.isFailed()) {
            throw r.getErr();
        }
    }

    void ToString::execute(VMState &vmState) {
        const Value srcReg = vmState.reg(src().index());
        Value &dstReg = vmState.regRef(dst().index());
        dstReg = srcReg;
        const auto r = dstReg.toString();
        if(r.isFailed()) {
            throw r.getErr();
        }
    }

    void LNot::execute(VMState &vmState) {
        const Value srcReg = vmState.reg(src().index());
        Value &dstReg = vmState.regRef(dst().index());
        dstReg = srcReg;
        dstReg.toLogicalNot();
    }

    void ChgThis::execute(VMState &vmState) {
        const auto &srcVal = vmState.reg(src().index());
        const auto &dstVal = vmState.reg(dst().index());
        dynamic_cast<Function *>(dstVal.asObject().unwrap())->thisObj = srcVal.asObject().unwrap();
    }

    void ChgSign::execute(VMState &vmState) {
        const Value srcReg = vmState.reg(src().index());
        Value &dstReg = vmState.regRef(dst().index());
        dstReg = srcReg;
        const auto r = dstReg.toSignChange();
        if(r.isFailed())
            throw r.getErr();
    }

    void ChkInv::execute(VMState &vmState) {
        const auto &srcVal = vmState.reg(src().index());
        Value &dstVal = vmState.regRef(dst().index());
        dstVal = Value{ dynamic_cast<DataObject *>(srcVal.asObject().unwrap())->isValid() };
    }

    void ChkIns::execute(VMState &vmState) {
        Value &dstVal = vmState.regRef(dst().index());
        if(const Value &srcVal = vmState.reg(src().index()); srcVal.isString()) {
            const Atom atom = vmState.context.rt().atomTable.intern(*srcVal.asString().value());
            dstVal = Value{ dstVal.asObject().unwrap()->instanceOf(atom) };
            return;
        }
        dstVal = Value{ false };
    }

    void Load::execute(VMState &vmState) {
        vmState.regRef(dst().index()) = vmState.curFrame()->chunk->getConstant(value()).createValue(&vmState.rt);
    }

    void ILoad::execute(VMState &vmState) { vmState.regRef(dst().index()) = Value{ value() }; }


    void DGlobal::execute(VMState &vmState) {
        const Value &srcVal = vmState.regRef(src().index());
        if(vmState.globalHas(atom()) && propObjectSet(vmState, srcVal, vmState.global(atom()))) {
            return;
        }

        vmState.global(atom(), Value{ srcVal });
    }

    void GGlobal::execute(VMState &vmState) {
        Value srcVal = vmState.global(atom());
        propObjectGet(vmState, srcVal, srcVal);
        vmState.regRef(dst().index()) = srcVal;
    }

    void GThis::execute(VMState &vmState) {
        Value srcVal = vmState.getThis(atom());
        propObjectGet(vmState, srcVal, srcVal);
        vmState.regRef(dst().index()) = srcVal;
    }

    void DThis::execute(VMState &vmState) {
        const Value &srcVal = vmState.regRef(src().index());

        if(vmState.hasThis(atom())) {
            if(propObjectSet(vmState, srcVal, vmState.getThis(atom()))) {
                return;
            }
        }

        vmState.setThis(atom(), srcVal);
    }

    void Mov::execute(VMState &vmState) {
        const Value &srcVal = vmState.reg(src().index());
        if(propObjectSet(vmState, srcVal, vmState.regRef(dst().index()))) {
            return;
        }
        vmState.regRef(dst().index()) = srcVal;
    }

    void CP::execute(VMState &vmState) {
        Value srcVal = vmState.reg(src().index());
        propObjectGet(vmState, srcVal, srcVal);
        vmState.regRef(dst().index()) = srcVal;
    }

    void Add::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        r3 = r1.add(r2).unwrap();
    }

    void Sub::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        r3 = r1.sub(r2).unwrap();
    }

    void Mul::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        auto r = r1.mul(r2);
        r3 = r.unwrap();
    }

    void Div::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        auto r = r1.div(r2);
        r3 = r.unwrap();
    }

    void Idiv::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        auto r = r1.idiv(r2);
        r3 = r.unwrap();
    }

    void Mod::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        auto r = r1.mod(r2);
        r3 = r.unwrap();
    }

    void BXor::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        r3 = r1.bitwiseXor(r2).unwrap();
    }

    void BOr::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        r3 = r1.bitwiseOr(r2).unwrap();
    }

    void BAnd::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        r3 = r1.bitwiseAnd(r2).unwrap();
    }

    void BlShift::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        r3 = r1.bitwiseLeftShift(r2).unwrap();
    }

    void BrShift::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        r3 = r1.bitwiseRightShift(r2).unwrap();
    }

    void BurShift::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        r3 = r1.bitwiseUnsignedRightShift(r2).unwrap();
    }

    void EQ::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        const bool r = r1.equals(r2);
        r3 = Value{ r };
    }

    void NEQ::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        const bool r = !r1.equals(r2);
        r3 = Value{ r };
    }

    void AbsEQ::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        const bool r = r1.discernEquals(r2);
        r3 = Value{ r };
    }

    void AbsNEQ::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        const bool r = !r1.discernEquals(r2);
        r3 = Value{ r };
    }

    void LT::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        const bool r = r1.littlerThan(r2).unwrap();
        r3 = Value{ r };
    }

    void LE::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        const bool r = !r1.greaterThan(r2).unwrap();
        r3 = Value{ r };
    }

    void GT::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        const bool r = r1.greaterThan(r2).unwrap();
        r3 = Value{ r };
    }

    void GE::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        const bool r = !r1.littlerThan(r2).unwrap();
        r3 = Value{ r };
    }

    void LAnd::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        const bool r = r1.logicalAnd(r2);
        r3 = Value{ r };
    }

    void LOr::execute(VMState &vmState) {
        const Value r1 = vmState.reg(src1().index());
        const Value r2 = vmState.reg(src2().index());
        Value &r3 = vmState.regRef(dst().index());
        const bool r = r1.logicalOr(r2);
        r3 = Value{ r };
    }

    void Call::execute(VMState &vmState) {
        const auto &object = vmState.reg(memberReg().index()).asObject().unwrap();
        object->call(vmState, dst().index(), argCount());
    }

    void GProp::execute(VMState &vmState) {
        const auto &val = vmState.reg(obj().index());
        const String &name = *vmState.reg(memberReg().index()).asString().unwrap();
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
                vmState.regRef(dst().index()) = global->getProp(atom);
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
                vmState.regRef(dst().index()) = tmp;
                return;
            }

            auto tmp = obj->getProp(atom);
            propObjectGet(vmState, tmp, tmp);
            vmState.regRef(dst().index()) = tmp;
            return;
        }

        // Native method
        const TypeId tId = ValueToTypeId::getId(val);
        assert(tId != TypeId::None);
        NativeFunction *nativeFn = vmState.context.findMethod(tId, atom);
        assert(nativeFn != nullptr);
        nativeFn = vmState.rt.create<NativeFunction>(*nativeFn).get();
        nativeFn->setThisObj(val);
        vmState.regRef(dst().index()) = Value{ nativeFn };
    }

    void DProp::execute(VMState &vmState) {
        const auto &r = vmState.reg(obj().index());
        CLL_ASSERT(r.isObject(), "gprop obj is not object");
        const String &name = *vmState.reg(memberReg().index()).asString().unwrap();
        const Atom atom = vmState.rt.atomTable.intern(name);
        r.asObject().unwrap()->setProp(atom, vmState.reg(src().index()));
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
                    c.add(fmt::format("{} = {}", r, vm->reg(r.index())));
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
            if(operand.type() != Operand::Type::None)
                return;
            out += " ";
            out += dumpOperand(operand).toStdStr();
            dumpOperandComment(c, operand, vm);
        };

        d(inst.getOp1());
        d(inst.getOp2());
        d(inst.getOp3());

        return dumpWithComment(out, c);
    }

#define DEF_AUTO_DUMP(className, instName)                                                                             \
    std::string className::dump(const VMState *vm) { return DumpInst::autoDump(instName, *this, vm); }

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
    DEF_AUTO_DUMP(Sub, "sub");
    DEF_AUTO_DUMP(Mul, "mul");
    DEF_AUTO_DUMP(Div, "div");
    DEF_AUTO_DUMP(Idiv, "idiv");
    DEF_AUTO_DUMP(Mod, "mod");

    // 全局与作用域
    DEF_AUTO_DUMP(DGlobal, "d_global");
    DEF_AUTO_DUMP(GGlobal, "g_global");
    DEF_AUTO_DUMP(Global, "global");

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

    namespace {
        // ---------------------- 无符号 LEB128 (uLEB128) ----------------------

        // 编码：把 uint64_t 值写到 vector<uint8_t> 的末尾，返回写入的字节数
        size_t encode_uleb128(uint64_t value, Vec<u8> &buffer) {
            const size_t start = buffer.size();
            do {
                u8 byte = value & 0x7F;
                value >>= 7;
                if(value != 0)
                    byte |= 0x80; // 延续位
                buffer.push_back(byte);
            } while(value != 0);
            return buffer.size() - start;
        }

        // 解码：从 pc 位置读取 uLEB128，返回值 + 消耗的字节数
        // 用 pair 返回 {value, bytes_read}
        std::pair<u64, size_t> decode_uleb128(const u8 *ptr, const size_t max_len) {
            u64 result = 0;
            size_t shift = 0;
            size_t bytes = 0;

            while(true) {
                if(bytes >= max_len) {
                    throw std::runtime_error("ULEB128 overflow or truncated");
                }
                const u8 byte = ptr[bytes++];
                result |= static_cast<u64>(byte & 0x7F) << shift;
                if((byte & 0x80) == 0)
                    break;
                shift += 7;
                if(shift >= 64) {
                    throw std::runtime_error("ULEB128 too large for uint64_t");
                }
            }
            return { result, bytes };
        }

        // ---------------------- 有符号 LEB128 (sLEB128) ----------------------

        // ZigZag 编码：把 int64_t 转为无符号表示（负数变奇数，正数变偶数）
        u64 zigzagEncode(const i64 value) {
            if(value >= 0)
                return static_cast<u64>(value) << 1;
            return (static_cast<u64>(-value) << 1) | 1;
        }

        // ZigZag 解码
        i64 zigzagDecode(const u64 value) {
            return (value & 1) ? -(static_cast<i64>(value >> 1)) : static_cast<i64>(value >> 1);
        }

        // 编码有符号数
        size_t encodeSleb128(const i64 value, Vec<u8> &buffer) { return encode_uleb128(zigzagEncode(value), buffer); }

        // 解码有符号数
        std::pair<i64, size_t> decodeSleb128(const u8 *ptr, const size_t max_len) {
            auto [uval, bytes] = decode_uleb128(ptr, max_len);
            return { zigzagDecode(uval), bytes };
        }

        // 写 u16（小端）
        void writeU16(Vec<u8> &buffer, const u16 value) {
            buffer.push_back(static_cast<u8>(value & 0xFF));
            buffer.push_back(static_cast<u8>(value >> 8));
        }

        // 读 u16（小端）
        u16 readU16(const u8 *ptr) { return static_cast<u16>(ptr[0]) | (static_cast<u16>(ptr[1]) << 8); }

    } // namespace

    // void NOP::encode(Vec<u8> &code) { code.push_back(static_cast<uint8_t>(OpCode::NOP)); }
    //
    // void Debugger::encode(Vec<u8> &code) { code.push_back(static_cast<uint8_t>(OpCode::Debugger)); }
    //
    // void Push::encode(Vec<u8> &code) {
    //     const uint16_t dst = getOp1Val<Register>().index();
    //     code.push_back(static_cast<uint8_t>(OpCode::Push));
    //     writeU16(code, dst);
    // }
    //
    // void PopN::encode(Vec<u8> &code) {
    //     const i64 imm = getOp1Val<Integer>();
    //     code.push_back(static_cast<uint8_t>(OpCode::PopN));
    //     encodeSleb128(imm, code);
    // }

    // void dispatch(const uint8_t* pc, size_t remaining) {
    //     Opcode op = static_cast<Opcode>(*pc++);
    //     remaining--;
    //
    //     switch (op) {
    //         case Opcode::ADD_REG_REG_REG: {
    //             if (remaining < 6) throw std::runtime_error("truncated");
    //             uint16_t dst  = read_u16(pc); pc += 2;
    //             uint16_t src1 = read_u16(pc); pc += 2;
    //             uint16_t src2 = read_u16(pc); pc += 2;
    //             // 执行 add regs[dst] = regs[src1] + regs[src2];
    //             break;
    //         }
    //         case Opcode::ADD_REG_REG_IMM: {
    //             if (remaining < 4) throw std::runtime_error("truncated");
    //             uint16_t dst = read_u16(pc); pc += 2;
    //             uint16_t src = read_u16(pc); pc += 2;
    //             remaining -= 4;
    //
    //             auto [imm, len] = decode_sleb128(pc, remaining);
    //             pc += len;
    //             // 执行 regs[dst] = regs[src] + imm;
    //             break;
    //         }
    //             // ...
    //     }
    // }
} // namespace cial::Inter
