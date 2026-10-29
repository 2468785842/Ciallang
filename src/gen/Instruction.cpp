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

// 指令语义(执行)已全部迁移到 src/vm/VMState.cpp 的扁平字节码解释器 run0，
// 本文件只保留 TAC 指令的 dump（调试输出）。

namespace cial::inter {
    using namespace cial::vm;

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
            if(operand.type() == Operand::Type::None)
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
    DEF_AUTO_DUMP(LoadImm, "load_imm");
    DEF_AUTO_DUMP(Push, "push");
    DEF_AUTO_DUMP(PopN, "pop_n");

    DEF_AUTO_DUMP(DLocal, "d_local");
    DEF_AUTO_DUMP(GLocal, "g_local");

    // 算术运算 (带状态输出)
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
    DEF_AUTO_DUMP(Asc, "asc");
    DEF_AUTO_DUMP(Chr, "chr");
    DEF_AUTO_DUMP(TypeOf, "type_of");

    // 逻辑与比较
    DEF_AUTO_DUMP(Inv, "inv");
    DEF_AUTO_DUMP(ChkInv, "chk_inv");
    DEF_AUTO_DUMP(EvalStr, "eval_str");
    DEF_AUTO_DUMP(ChkIns, "chk_ins");
    DEF_AUTO_DUMP(TestVoid, "test_void");
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

    std::string CompareTest::dump(const VMState *) {
        return fmt::format("compare_test {} %{} %{} %{}", static_cast<int>(comparison), getOp1Val<Register>().index(),
                           getOp2Val<Register>().index(), getOp3Val<Register>().index());
    }
    std::string GMethodC::dump(const VMState *) {
        return fmt::format("g_method_c %{} atom_{} %{} %{}", getOp1Val<Register>().index(), getOp2Val<Atom>().v,
                           getOp3Val<Register>().index(), receiver.index());
    }
    std::string CallMethodV::dump(const VMState *vm) {
        return fmt::format("call_method receiver=%{} {}", receiver.index(), CallV::dump(vm));
    }

    std::string CallV::dump(const VMState *vm) {
        std::string out = fmt::format("call_v     {:<4} {:<4} {}  ", func.index(), dst.index(),
                                      static_cast<i64>(argCount));
        for(const auto &r : args)
            out += fmt::format("%{} ", r.index());
        return out;
    }

    std::string CallOmit::dump(const VMState *vm) {
        return fmt::format("call_omit  {:<4} {:<4}", func.index(), dst.index());
    }

    std::string CallExpand::dump(const VMState *vm) {
        std::string out = fmt::format("call_expd  {:<4} {:<4} base={} ", func.index(),
                                      dst.index(), static_cast<i64>(unnamedBase));
        static constexpr std::string_view names[]{"n", "e", "u"};
        for(const auto &[t, r] : entries)
            out += fmt::format("{}:%{} ", names[t < 3 ? t : 0], r.index());
        return out;
    }

    std::string CollapseArgs::dump(const VMState *vm) {
        return fmt::format("collapse   {:<4} base={}", dst.index(), static_cast<i64>(base));
    }

    DEF_AUTO_DUMP(CmpEQ, "cmp_eq");
    DEF_AUTO_DUMP(CmpDEQ, "cmp_deq");
    DEF_AUTO_DUMP(CmpLT, "cmp_lt");
    DEF_AUTO_DUMP(CmpGT, "cmp_gt");
    DEF_AUTO_DUMP(Inc, "inc");
    DEF_AUTO_DUMP(Dec, "dec");
    DEF_AUTO_DUMP(BNot, "b_not");
    DEF_AUTO_DUMP(ToNumber, "to_number");
    DEF_AUTO_DUMP(ToOctet, "to_octet");
    DEF_AUTO_DUMP(Ret, "ret");

    // 属性操作
    DEF_AUTO_DUMP(GProp, "g_prop");
    DEF_AUTO_DUMP(DProp, "d_prop");
    DEF_AUTO_DUMP(DelProp, "del_prop");
    DEF_AUTO_DUMP(GPropRaw, "g_prop_raw");
    DEF_AUTO_DUMP(GRefRaw, "g_ref_raw");
    DEF_AUTO_DUMP(DPropRaw, "d_prop_raw");
    DEF_AUTO_DUMP(DerefGet, "deref_get");
    DEF_AUTO_DUMP(DerefSet, "deref_set");
    DEF_AUTO_DUMP(GPropC, "g_prop_c");
    DEF_AUTO_DUMP(DPropC, "d_prop_c");
    DEF_AUTO_DUMP(GPropRawC, "g_prop_raw_c");
    DEF_AUTO_DUMP(DPropRawC, "d_prop_raw_c");
    DEF_AUTO_DUMP(DelPropC, "del_prop_c");
    DEF_AUTO_DUMP(NewArray, "new_array");
    DEF_AUTO_DUMP(NewDict, "new_dict");
    DEF_AUTO_DUMP(GetIndex, "get_index");
    DEF_AUTO_DUMP(SetIndex, "set_index");
    DEF_AUTO_DUMP(WithPush, "with_push");
    DEF_AUTO_DUMP(WithPop, "with_pop");
    DEF_AUTO_DUMP(GetWith, "get_with");
    DEF_AUTO_DUMP(NewRegExp, "new_regexp");

    // 其他
    DEF_AUTO_DUMP(ChgSign, "chg_sign");
    DEF_AUTO_DUMP(Throw, "throw");
    DEF_AUTO_DUMP(Debugger, "debugger");
} // namespace cial::inter
