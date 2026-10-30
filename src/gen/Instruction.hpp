/*
 * Copyright (c) 2024/5/30 上午8:07
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
#pragma once

#include "Label.hpp"
#include "Register.hpp"
#include "runtime/AtomTable.hpp"
#include "vm/Constant.hpp"

// clang-format off
#define CIAL_TAC_OPCODE_ENUMS(O) \
    O(NOP) \
    O(Debugger) \
    O(Push) \
    O(PopN) \
    O(Global) \
    O(Super) \
    O(This) \
    O(Test) \
    O(Inv) \
    O(Throw) \
    O(Ret) \
    O(Jmp) \
    O(JmpE) \
    O(JmpNE) \
    O(ToInt) \
    O(ToReal) \
    O(Asc) \
    O(Chr) \
    O(ToString) \
    O(TypeOf) \
    O(LNot) \
    O(ChgSign) \
    O(ChgThis) \
    O(ChkInv) \
    O(EvalStr) \
    O(ChkIns) \
    O(TestVoid) \
    O(Load) \
    O(LoadImm) \
    O(DGlobal) \
    O(GGlobal) \
    O(GThis) \
    O(DThis) \
    O(GLocal) \
    O(DLocal) \
    O(Add) \
    O(Sub) \
    O(Mul) \
    O(Div) \
    O(Idiv) \
    O(Mod) \
    O(BXor) \
    O(BOr) \
    O(BAnd) \
    O(BlShift) \
    O(BrShift) \
    O(BurShift) \
    O(EQ) \
    O(NEQ) \
    O(AbsEQ) \
    O(AbsNEQ) \
    O(LT) \
    O(LE) \
    O(GT) \
    O(GE) \
    O(LAnd) \
    O(LOr) \
    O(Call) \
    O(GProp) \
    O(DProp) \
    O(DelProp) \
    O(GPropRaw) \
    O(GRefRaw) \
    O(DPropRaw) \
    O(DerefGet) \
    O(DerefSet) \
    O(NewArray) \
    O(NewDict) \
    O(GetIndex) \
    O(SetIndex) \
    O(WithPush) \
    O(WithPop) \
    O(GetWith) \
    O(NewRegExp) \
    O(CallV) \
    O(CallOmit) \
    O(CallExpand) \
    O(CollapseArgs) \
    O(CmpEQ) \
    O(CmpDEQ) \
    O(CmpLT) \
    O(CmpGT) \
    O(Inc) \
    O(Dec) \
    O(BNot) \
    O(ToNumber) \
    O(ToOctet) \
    O(GPropC) \
    O(DPropC) \
    O(GPropRawC) \
    O(DPropRawC) \
    O(DelPropC) \
    O(CompareTest) \
    O(GMethodC) \
    O(CallMethodV)
// clang-format on

namespace cial::vm {
    class VMState;
    class Bytecode;
} // namespace cial::vm

namespace cial::inter {
    enum class TacOpCode : u8 {
#define OPCODE_ENUM_CLASS(OP) OP,
        CIAL_TAC_OPCODE_ENUMS(OPCODE_ENUM_CLASS)
#undef OPCODE_ENUM_CLASS
    };

    class Instruction;
    struct DumpInst;

    struct Operand {
        enum class Type { None, Register, Label, ConstIndex, Number, Atom };

        explicit Operand() = default;

        template <typename T>
        explicit Operand(T operand) : _operand{ operand } {}

        [[nodiscard]] Type type() const noexcept { return static_cast<Type>(_operand.index()); }

        template <typename T>
        [[nodiscard]] T value() const noexcept {
            assert(std::holds_alternative<T>(_operand));
            return std::get<T>(_operand);
        }

        bool operator==(const Register &reg) const noexcept {
            if(!std::holds_alternative<Register>(_operand))
                return false;
            return std::get<Register>(_operand) == reg;
        }

    private:
        using Value = std::variant<std::monostate, Register, Label, ConstIdx, Integer, Atom>;
        Value _operand;
    };

    class Instruction {
    public:
        explicit Instruction() = default;

        virtual ~Instruction() = default;

        Instruction(const Instruction &) = default;

        Instruction &operator=(const Instruction &) = default;

        Instruction(Instruction &&) = default;

        Instruction &operator=(Instruction &&) = default;

        [[nodiscard]] virtual TacOpCode opcode() const = 0;

        [[nodiscard]] auto getOp1() const { return _ops[0]; }
        [[nodiscard]] auto getOp2() const { return _ops[1]; }
        [[nodiscard]] auto getOp3() const { return _ops[2]; }

        template <typename T>
        void setOp1Val(T &&v) {
            _ops[0] = Operand{ std::forward<T>(v) };
        }

        template <typename T>
        void setOp2Val(T &&v) {
            _ops[1] = Operand{ std::forward<T>(v) };
        }

        template <typename T>
        void setOp3Val(T &&v) {
            _ops[2] = Operand{ std::forward<T>(v) };
        }

        template <typename T>
        [[nodiscard]] constexpr T getOp1Val() const {
            return _ops[0].value<T>();
        }

        template <typename T>
        [[nodiscard]] constexpr T getOp2Val() const {
            return _ops[1].value<T>();
        }

        template <typename T>
        [[nodiscard]] constexpr T getOp3Val() const {
            return _ops[2].value<T>();
        }

        // TAC 指令用于中间表示和调试输出。
        virtual std::string dump(const vm::VMState *vmState) = 0;

    protected:
        Operand _ops[3];
        friend struct DumpInst;
    };

#define DECLARE_TAC_OPCODE_CLASS(name) class name;
    CIAL_TAC_OPCODE_ENUMS(DECLARE_TAC_OPCODE_CLASS)
#undef DECLARE_TAC_OPCODE_CLASS

// 操作码
#define OP_GET1(name, type)                                                                                            \
    type name() const { return getOp1Val<type>(); }
#define OP_GET2(name, type)                                                                                            \
    type name() const { return getOp2Val<type>(); }
#define OP_GET3(name, type)                                                                                            \
    type name() const { return getOp3Val<type>(); }

#define DEF_CTOR_0(className)                                                                                          \
    explicit className() : Instruction() {}

#define DEF_CTOR_1(className)                                                                                          \
    explicit className(Operand o1) : Instruction() { _ops[0] = o1; }

#define DEF_CTOR_2(className)                                                                                          \
    explicit className(Operand o1, Operand o2) : Instruction() {                                                       \
        _ops[0] = o1;                                                                                                  \
        _ops[1] = o2;                                                                                                  \
    }

#define DEF_CTOR_3(className)                                                                                          \
    explicit className(Operand o1, Operand o2, Operand o3) : Instruction() {                                           \
        _ops[0] = o1;                                                                                                  \
        _ops[1] = o2;                                                                                                  \
        _ops[2] = o3;                                                                                                  \
    }

#define DEF_INST(className, N, getters)                                                                                \
    struct className : Instruction {                                                                                   \
        DEF_CTOR_##N(className) virtual ~className() = default;                                                        \
        [[nodiscard]] TacOpCode opcode() const override { return TacOpCode::className; }                               \
        getters [[nodiscard]] std::string dump(const vm::VMState *vmState) override;                                   \
    }

    // ────────────────────────────────────────────────
    // 定义指令
    // ────────────────────────────────────────────────
    DEF_INST(NOP, 0, /* no getters */);
    DEF_INST(Debugger, 0, );
    DEF_INST(Push, 1, OP_GET1(src, Register));
    DEF_INST(PopN, 1, OP_GET1(count, Integer));

    DEF_INST(Global, 1, OP_GET1(dst, Register));
    DEF_INST(Super, 1, OP_GET1(dst, Register));
    DEF_INST(This, 1, OP_GET1(dst, Register));

    DEF_INST(Test, 1, OP_GET1(src, Register));

    DEF_INST(Inv, 1, OP_GET1(src, Register));

    DEF_INST(Throw, 1, OP_GET1(src, Register));

    DEF_INST(Ret, 1, OP_GET1(src, Register));

    DEF_INST(Jmp, 1, OP_GET1(label, Label));
    DEF_INST(JmpE, 1, OP_GET1(label, Label));
    DEF_INST(JmpNE, 1, OP_GET1(label, Label));

    DEF_INST(ToInt, 2, OP_GET1(src, Register) OP_GET2(dst, Register));
    DEF_INST(ToReal, 2, OP_GET1(src, Register) OP_GET2(dst, Register));
    DEF_INST(ToString, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(TypeOf, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // #s: 字符串首字符的字符码(旧版 VM_ASC);$n: 字符码转 1 字符字符串(旧版 VM_CHR)
    DEF_INST(Asc, 2, OP_GET1(src, Register) OP_GET2(dst, Register));
    DEF_INST(Chr, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(LNot, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(ChgSign, 2, OP_GET1(src, Register) OP_GET2(dst, Register));
    DEF_INST(ChgThis, 2, OP_GET1(dst, Register) OP_GET2(src, Register));

    DEF_INST(ChkInv, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // 后缀 `!` 即时求值（tjs.y priority_expr "!" → T_EVAL）：src 字符串化为
    // TJS 表达式编译执行，结果写 dst；空串/非串原值不变（原版 Eval 语义）
    DEF_INST(EvalStr, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(ChkIns, 2, OP_GET1(dst, Register) OP_GET2(src, Register));

    // src 是否为 void → dst(bool)；默认参数守卫用（实参未传时才求值默认表达式）
    DEF_INST(TestVoid, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(Load, 2, OP_GET1(dst, Register) OP_GET2(value, ConstIdx));
    DEF_INST(LoadImm, 2, OP_GET1(dst, Register) OP_GET2(value, Integer));

    DEF_INST(DGlobal, 2, OP_GET1(atom, Atom) OP_GET2(src, Register));
    DEF_INST(GGlobal, 2, OP_GET1(atom, Atom) OP_GET2(dst, Register));
    DEF_INST(GThis, 2, OP_GET1(atom, Atom) OP_GET2(dst, Register));
    DEF_INST(DThis, 2, OP_GET1(atom, Atom) OP_GET2(src, Register));

    DEF_INST(DLocal, 2, OP_GET1(src, Register) OP_GET2(dst, Register));
    DEF_INST(GLocal, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(Add, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(Sub, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(Mul, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(Div, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(Idiv, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(Mod, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));

    DEF_INST(BXor, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(BOr, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(BAnd, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(BlShift, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(BrShift, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(BurShift, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));

    DEF_INST(EQ, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(NEQ, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(AbsEQ, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(AbsNEQ, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(LT, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(LE, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(GT, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(GE, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(LAnd, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));
    DEF_INST(LOr, 3, OP_GET1(src1, Register) OP_GET2(src2, Register) OP_GET3(dst, Register));

    DEF_INST(Call, 3, OP_GET1(dst, Register) OP_GET2(memberReg, Register) OP_GET3(argCount, Integer));
    DEF_INST(GProp, 3, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(dst, Register));
    DEF_INST(DProp, 3, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(src, Register));
    // delete obj.member:删除成员,dst 写入是否删除成功(bool)
    DEF_INST(DelProp, 3, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(dst, Register));
    // &obj.member:取成员本体(property 不触发 getter,旧版 GPDS)
    DEF_INST(GPropRaw, 3, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(dst, Register));
    // &obj.member = v:写槽位本体,绕过 setter(旧版 SPDS);& 作赋值左值时用
    DEF_INST(DPropRaw, 3, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(src, Register));
    // &identifier:取 this/global 槽位本体,不触发 getter(旧版 GPDS 的符号形态)
    DEF_INST(GRefRaw, 2, OP_GET1(atom, Atom) OP_GET2(dst, Register));
    // *p 读:强制属性访问(调 getter,旧版 GETP);非 Property 报错
    DEF_INST(DerefGet, 2, OP_GET1(prop, Register) OP_GET2(dst, Register));
    // *p = v 写:强制属性访问(调 setter,旧版 SETP);setter 压帧,需 Call 接管
    DEF_INST(DerefSet, 2, OP_GET1(prop, Register) OP_GET2(src, Register));

    // ── 成员名 Atom 直引族（C 后缀 = 编译期常量成员名）──
    // 成员名以 u32 Atom 内联进指令流（同 GGlobal），dispatch 免去
    // 「常量池 Atom → String Value → intern 回 Atom」的往返哈希。
    // 寄存器成员名形态（GProp/DProp/...）保留：动态成员名与整数成员名
    //（旧版 GPI/PropGetByNum，数组下标）仍走寄存器
    // obj.member 读（getter 语义与 GProp 一致）
    DEF_INST(GPropC, 3, OP_GET1(obj, Register) OP_GET2(atom, Atom) OP_GET3(dst, Register));
    // obj.member = v 写（DProp 语义：Property 槽位触发 setter，压帧 → Call）
    DEF_INST(DPropC, 3, OP_GET1(obj, Register) OP_GET2(atom, Atom) OP_GET3(src, Register));
    // &obj.member 读（GPropRaw 语义：不触发 getter）
    DEF_INST(GPropRawC, 3, OP_GET1(obj, Register) OP_GET2(atom, Atom) OP_GET3(dst, Register));
    // &obj.member = v 写（DPropRaw 语义：绕过 setter）
    DEF_INST(DPropRawC, 3, OP_GET1(obj, Register) OP_GET2(atom, Atom) OP_GET3(src, Register));
    // delete obj.member（DelProp 语义：dst 写是否删除成功）
    DEF_INST(DelPropC, 3, OP_GET1(obj, Register) OP_GET2(atom, Atom) OP_GET3(dst, Register));

    // [a, b, ...] 字面量:分配 count 长度的新 Array,元素随后逐个 SetIndex
    DEF_INST(NewArray, 2, OP_GET1(dst, Register) OP_GET2(count, Integer));
    // %[...] 字面量:新建空 Dictionary
    DEF_INST(NewDict, 1, OP_GET1(dst, Register));
    // a[i] 读:数组整数下标(负=从尾)、字典键、字符串字符(旧 charAt 语义)
    DEF_INST(GetIndex, 3, OP_GET1(obj, Register) OP_GET2(idxReg, Register) OP_GET3(dst, Register));
    // a[i] = v 写:数组越界自动扩展补 void;字典键写入
    DEF_INST(SetIndex, 3, OP_GET1(obj, Register) OP_GET2(idxReg, Register) OP_GET3(src, Register));

    // with(obj) 语句:压入 with 上下文栈(裸词链 .member 的查找基座);
    // withStack 挂 Runtime handle 保 GC
    DEF_INST(WithPush, 1, OP_GET1(src, Register));
    DEF_INST(WithPop, 0, );
    // 裸词链基座 .member:取 with 栈顶对象,空栈得 global
    DEF_INST(GetWith, 1, OP_GET1(dst, Register));
    // /pattern/ 正则字面量:按 String 常量取模式,构造 RegExp 对象
    DEF_INST(NewRegExp, 2, OP_GET1(dst, Register) OP_GET2(pattern, ConstIdx));

    // 变长实参调用：语义 = N×Push + Call（消逐参派发轮次）。Operand 三槽装不下
    // 寄存器列表，手写子类；args 为实参寄存器（缺位实参为 void 常量寄存器）
    class CallV : public Instruction {
    public:
        Register dst{};
        Register func{};
        Integer argCount{};
        Vec<Register> args{};

        CallV(const Register d, const Register f, const Integer n, Vec<Register> a) :
            dst(d), func(f), argCount(n), args(std::move(a)) {}

        ~CallV() override = default;

        [[nodiscard]] TacOpCode opcode() const override { return TacOpCode::CallV; }

        [[nodiscard]] std::string dump(const vm::VMState *vmState) override;
    };

    class CompareTest : public Instruction {
    public:
        TacOpCode comparison;
        CompareTest(TacOpCode op, Register lhs, Register rhs, Register dst) : comparison(op) {
            _ops[0] = Operand{ lhs };
            _ops[1] = Operand{ rhs };
            _ops[2] = Operand{ dst };
        }
        TacOpCode opcode() const override { return TacOpCode::CompareTest; }
        std::string dump(const vm::VMState *) override;
    };

    // 方法读取先于实参求值。仅原生方法把接收者分开存放，避免绑定对象分配；
    // getter/脚本方法等回落到原有属性读取协议，receiver 留 void。
    class GMethodC : public Instruction {
    public:
        Register receiver;
        GMethodC(Register obj, Atom atom, Register dst, Register receiver) : receiver(receiver) {
            _ops[0] = Operand{ obj };
            _ops[1] = Operand{ atom };
            _ops[2] = Operand{ dst };
        }
        TacOpCode opcode() const override { return TacOpCode::GMethodC; }
        std::string dump(const vm::VMState *) override;
    };

    class CallMethodV : public CallV {
    public:
        Register receiver;
        CallMethodV(Register dst, Register func, Integer n, Vec<Register> args, Register receiver) :
            CallV(dst, func, n, std::move(args)), receiver(receiver) {}
        TacOpCode opcode() const override { return TacOpCode::CallMethodV; }
        std::string dump(const vm::VMState *) override;
    };

    // 省略透传调用（旧版 argc=-1，TJS `f(...)`）：把当前帧的实际实参
    //（%0..%actualArgc-1）原样转发给 func。actualArgc 由 CallFrame 记录
    class CallOmit : public Instruction {
    public:
        Register dst{};
        Register func{};

        CallOmit(const Register d, const Register f) : dst(d), func(f) {}

        ~CallOmit() override = default;

        [[nodiscard]] TacOpCode opcode() const override { return TacOpCode::CallOmit; }

        [[nodiscard]] std::string dump(const vm::VMState *vmState) override;
    };

    // 展开调用（旧版 argc=-2，TJS `f(1, arr*, *)`）：entries 为
    // (fatType, 寄存器) 表——0=Normal 直传、1=Expand 展开 Array 元素、
    // 2=UnnamedExpand 转发本帧 unnamed 实参（%unnamedBase..%actualArgc-1）。
    // 与 CallV 同款手写子类
    class CallExpand : public Instruction {
    public:
        Register dst{};
        Register func{};
        Integer unnamedBase{};
        Vec<std::pair<u8, Register>> entries{};

        CallExpand(const Register d, const Register f, const Integer base,
                   Vec<std::pair<u8, Register>> e) :
            dst(d), func(f), unnamedBase(base), entries(std::move(e)) {}

        ~CallExpand() override = default;

        [[nodiscard]] TacOpCode opcode() const override { return TacOpCode::CallExpand; }

        [[nodiscard]] std::string dump(const vm::VMState *vmState) override;
    };

    // collapse 参数收拢（旧版 FuncDeclCollapseBase，TJS `function f(a, b*)`）：
    // 把本帧实际实参 %base..%actualArgc-1 收拢成新 Array 写 dst（TJS 位置
    // ra[-3-base]）；无超额实参时写空 Array（TJS2 无条件建 Array）
    class CollapseArgs : public Instruction {
    public:
        Register dst{};
        Integer base{};

        CollapseArgs(const Register d, const Integer b) : dst(d), base(b) {}

        ~CollapseArgs() override = default;

        [[nodiscard]] TacOpCode opcode() const override { return TacOpCode::CollapseArgs; }

        [[nodiscard]] std::string dump(const vm::VMState *vmState) override;
    };

    // ── M3：比较直写 _zf（省 %F 物化与 Test；2 操作数，无 dst） ──
    // CmpEQ/CmpDEQ/CmpLT/CmpGT ↔ 旧版 CEQ/CDEQ/CLT/CGT（自然序，见转译器注）
    DEF_INST(CmpEQ, 2, OP_GET1(src1, Register) OP_GET2(src2, Register));
    DEF_INST(CmpDEQ, 2, OP_GET1(src1, Register) OP_GET2(src2, Register));
    DEF_INST(CmpLT, 2, OP_GET1(src1, Register) OP_GET2(src2, Register));
    DEF_INST(CmpGT, 2, OP_GET1(src1, Register) OP_GET2(src2, Register));

    // 自增/自减单指令（旧版 INC/DEC 基型）：int ±1 / real ±1.0，其余报错
    //（旧译法 LoadImm+Add 对字符串会拼接，TJS increment 语义应报错）
    DEF_INST(Inc, 1, OP_GET1(src, Register));
    DEF_INST(Dec, 1, OP_GET1(src, Register));

    // 按位非（旧版 BNOT）：~asInteger（real 截断、字符串解析，与 TJS AsInteger 链一致）
    DEF_INST(BNot, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // 数值规范化（旧版 NUM/tonumber）：int/real 不变、字符串解析为数值、
    // void → 0、其余报错
    DEF_INST(ToNumber, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // 转 octet（旧版 OCTET/ToOctet）：void → 空 octet、octet 不变、其余报错
    DEF_INST(ToOctet, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    struct DumpInst {
        [[nodiscard]] static String dumpOperand(const Operand &operand);
        [[nodiscard]] static std::string autoDump(std::string_view name, const Instruction &inst,
                                                  const vm::VMState *vm);
    };

}; // namespace cial::inter
