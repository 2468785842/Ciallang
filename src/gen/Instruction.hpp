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

namespace cial::Bytecode {
    class VMState;
}

namespace cial::Inter {
#define OPCODE_ENUMS(O)                                                                                                \
    O(NOP)                                                                                                             \
    O(Load)                                                                                                            \
    O(ILoad)                                                                                                           \
    O(Push)                                                                                                            \
    O(PopN)                                                                                                            \
    O(CP)                                                                                                              \
    O(Add)                                                                                                             \
    O(IAdd)                                                                                                            \
    O(Sub)                                                                                                             \
    O(ISub)                                                                                                            \
    O(Mul)                                                                                                             \
    O(Div)                                                                                                             \
    O(Idiv)                                                                                                            \
    O(Mod)                                                                                                             \
    O(Mov)                                                                                                             \
    O(DGlobal)                                                                                                         \
    O(GGlobal)                                                                                                         \
    O(Global)                                                                                                          \
    O(Super)                                                                                                           \
    O(This)                                                                                                            \
    O(ToInt)                                                                                                           \
    O(ToReal)                                                                                                          \
    O(ToString)                                                                                                        \
    O(ChgThis)                                                                                                         \
    O(Inv)                                                                                                             \
    O(ChkInv)                                                                                                          \
    O(ChkIns)                                                                                                          \
    O(Test)                                                                                                            \
    O(EQ)                                                                                                              \
    O(NEQ)                                                                                                             \
    O(LT)                                                                                                              \
    O(LE)                                                                                                              \
    O(GT)                                                                                                              \
    O(GE)                                                                                                              \
    O(AbsEQ)                                                                                                           \
    O(AbsNEQ)                                                                                                          \
    O(Jmp)                                                                                                             \
    O(JmpE)                                                                                                            \
    O(JmpNE)                                                                                                           \
    O(Call)                                                                                                            \
    O(GProp)                                                                                                           \
    O(DProp)                                                                                                           \
    O(GUpval)                                                                                                          \
    O(GThis)                                                                                                           \
    O(DThis)                                                                                                           \
    O(LNot)                                                                                                            \
    O(LAnd)                                                                                                            \
    O(LOr)                                                                                                             \
    O(BXor)                                                                                                            \
    O(BOr)                                                                                                             \
    O(BAnd)                                                                                                            \
    O(BlShift)                                                                                                         \
    O(BrShift)                                                                                                         \
    O(BurShift)                                                                                                        \
    O(ChgSign)                                                                                                         \
    O(Debugger)                                                                                                        \
    O(Throw)                                                                                                           \
    O(Ret)

    enum class OpCode : u16 {
#define OPCODE_ENUM_CLASS(OP) OP,
        OPCODE_ENUMS(OPCODE_ENUM_CLASS)
#undef OPCODE_ENUM_CLASS
            COUNT
    };

    class Instruction;

    using ExecuteCallback = void (*)(const Instruction &, Bytecode::VMState &);
    using DumpCallback = std::string (*)(const Instruction &, const Bytecode::VMState &, bool);

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

        [[nodiscard]] virtual OpCode opcode() const = 0;

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

        virtual void execute(Bytecode::VMState &vmState) = 0;

        virtual std::string dump(const Bytecode::VMState *vmState) = 0;

    protected:
        Operand _ops[3];
        friend struct DumpInst;
    };

// 操作码
#define OP_GET1(name, type)                                                                                            \
    type name() const { return getOp1Val<type>(); }
#define OP_GET2(name, type)                                                                                            \
    type name() const { return getOp2Val<type>(); }
#define OP_GET3(name, type)                                                                                            \
    type name() const { return getOp3Val<type>(); }

#define PP_NARG(...) PP_NARG_(__VA_ARGS__, PP_RSEQ_N())
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define PP_RSEQ_N() 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

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
        [[nodiscard]] OpCode opcode() const override { return OpCode::className; }                                     \
        getters void execute(Bytecode::VMState &vmState) override;                                                     \
        [[nodiscard]] std::string dump(const Bytecode::VMState *vmState) override;                                     \
    }

    // ────────────────────────────────────────────────
    // 定义指令
    // ────────────────────────────────────────────────

    DEF_INST(NOP, 0, /* no getters */);

    DEF_INST(Debugger, 0, );

    // use op1
    DEF_INST(Push, 1, OP_GET1(src, Register));

    DEF_INST(PopN, 1, OP_GET1(count, Integer));

    // def op1
    DEF_INST(Global, 1, OP_GET1(dst, Register));

    // def op1
    DEF_INST(Super, 1, OP_GET1(dst, Register));

    // def op1
    DEF_INST(This, 1, OP_GET1(dst, Register));

    // def op1
    DEF_INST(ToInt, 1, OP_GET1(dst, Register));

    // def op1
    DEF_INST(ToReal, 1, OP_GET1(dst, Register));

    // def op1
    DEF_INST(ToString, 1, OP_GET1(dst, Register));

    // use op1
    DEF_INST(Inv, 1, OP_GET1(dst, Register));

    // use op1
    DEF_INST(Test, 1, OP_GET1(reg, Register));

    // use op1 def op1
    DEF_INST(LNot, 1, OP_GET1(dst, Register));

    // use op1 def op1
    DEF_INST(ChgSign, 1, OP_GET1(dst, Register));

    // use op1
    DEF_INST(Throw, 1, OP_GET1(src, Register));

    // use op1
    DEF_INST(Ret, 1, OP_GET1(retReg, Register));

    DEF_INST(Jmp, 1, OP_GET1(label, Label));

    DEF_INST(JmpE, 1, OP_GET1(label, Label));

    DEF_INST(JmpNE, 1, OP_GET1(label, Label));

    // def op1
    DEF_INST(Load, 2, OP_GET1(dst, Register) OP_GET2(value, ConstIdx));

    // def op1
    DEF_INST(ILoad, 2, OP_GET1(dst, Register) OP_GET2(value, Integer));

    // use op1 op2 def op2
    DEF_INST(Add, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(IAdd, 2, OP_GET1(src, Integer) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(Sub, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(ISub, 2, OP_GET1(src, Integer) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(Mul, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(Div, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(Idiv, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(Mod, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 def op2
    DEF_INST(Mov, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 def op2
    DEF_INST(CP, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op2
    DEF_INST(DGlobal, 2, OP_GET1(atom, Atom) OP_GET2(src, Register));

    // def op2
    DEF_INST(GGlobal, 2, OP_GET1(atom, Atom) OP_GET2(dst, Register));

    // use op1 op2
    DEF_INST(ChgThis, 2, OP_GET1(dst, Register) OP_GET2(src, Register));

    // use op1 op2 def op2
    DEF_INST(ChkInv, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op1
    DEF_INST(ChkIns, 2, OP_GET1(dst, Register) OP_GET2(src, Register));

    // use op1 op2 def op2
    DEF_INST(EQ, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(NEQ, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(LT, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(LE, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(GT, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(GE, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(AbsEQ, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(AbsNEQ, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(LAnd, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // def op2
    DEF_INST(GThis, 2, OP_GET1(atom, Atom) OP_GET2(dst, Register));

    // use op2
    DEF_INST(DThis, 2, OP_GET1(atom, Atom) OP_GET2(src, Register));

    // def op2
    DEF_INST(GUpval, 2, OP_GET1(atom, Atom) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(LOr, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BXor, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BOr, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BAnd, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BlShift, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BrShift, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BurShift, 2, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op2 def op1
    DEF_INST(Call, 3, OP_GET1(dst, Register) OP_GET2(memberReg, Register) OP_GET3(argCount, Integer));

    // use op1 op2 def op3
    DEF_INST(GProp, 3, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(dst, Register));

    // use op1 op2 op3
    DEF_INST(DProp, 3, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(src, Register));

    struct DumpInst {
        [[nodiscard]] static String dumpOperand(const Operand &operand);
        [[nodiscard]] static std::string autoDump(std::string_view name, const Instruction &inst,
                                                  const Bytecode::VMState *vm);
    };
}; // namespace cial::Inter
