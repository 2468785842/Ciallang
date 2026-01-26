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

#include <optional>

#include "Constant.hpp"
#include "Label.hpp"
#include "Register.hpp"
#include "logging/Logger.hpp"
#include "runtime/AtomTable.hpp"

namespace cial::Bytecode {
    class VMState;

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

    using ExecuteCallback = void (*)(const Instruction &, VMState &);
    using DumpCallback = std::string (*)(const Instruction &, const VMState &, bool);

    struct Operand {
        enum class Type { None, Register, Label, ConstIndex, Number, Atom };

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
        explicit Instruction(const OpCode opcode) : _opcode(opcode) {}
        explicit Instruction(const OpCode opcode, Operand operand) : _opcode(opcode), _operand1(operand) {}
        explicit Instruction(const OpCode opcode, Operand operand1, Operand operand2) :
            _opcode(opcode), _operand1(operand1), _operand2(operand2) {}
        explicit Instruction(const OpCode opcode, Operand operand1, Operand operand2, Operand operand3) :
            _opcode(opcode), _operand1(operand1), _operand2(operand2), _operand3(operand3) {}

        Instruction(const Instruction &) = default;

        Instruction &operator=(const Instruction &) = default;

        Instruction(Instruction &&) = default;

        Instruction &operator=(Instruction &&) = default;

        [[nodiscard]] OpCode opcode() const { return _opcode; }

        [[nodiscard]] auto getOp1() const { return _operand1; }
        [[nodiscard]] auto getOp2() const { return _operand2; }
        [[nodiscard]] auto getOp3() const { return _operand3; }

        template <typename T>
        void setOp1Val(T &&v) {
            _operand1 = Operand{ std::forward<T>(v) };
        }

        template <typename T>
        void setOp2Val(T &&v) {
            _operand2 = Operand{ std::forward<T>(v) };
        }

        template <typename T>
        void setOp3Val(T &&v) {
            _operand3 = Operand{ std::forward<T>(v) };
        }

        template <typename T>
        [[nodiscard]] constexpr T getOp1Val() const {
            CLL_ASSERT(_operand1, "operand1 is empty");
            return _operand1->value<T>();
        }

        template <typename T>
        [[nodiscard]] constexpr T getOp2Val() const {
            CLL_ASSERT(_operand2, "operand2 is empty");
            return _operand2->value<T>();
        }

        template <typename T>
        [[nodiscard]] constexpr T getOp3Val() const {
            CLL_ASSERT(_operand3, "operand3 is empty");
            return _operand3->value<T>();
        }

        [[nodiscard]] Integer operandCount() const {
            return static_cast<int>(_operand1.has_value()) + static_cast<int>(_operand2.has_value()) +
                static_cast<int>(_operand3.has_value());
        }

        static void execute(const Instruction &inst, VMState &vmState);

        static std::string dump(const Instruction &inst, const VMState *vmState = nullptr);

    private:
        OpCode _opcode;
        std::optional<Operand> _operand1;
        std::optional<Operand> _operand2;
        std::optional<Operand> _operand3;
        friend struct DumpInst;
    };

// 操作码
#define OP_GET1(name, type)                                                                                            \
    static type name(const Instruction &inst) { return inst.getOp1Val<type>(); }
#define OP_GET2(name, type)                                                                                            \
    static type name(const Instruction &inst) { return inst.getOp2Val<type>(); }
#define OP_GET3(name, type)                                                                                            \
    static type name(const Instruction &inst) { return inst.getOp3Val<type>(); }

// 定义指令
#define DEF_INST_EX(className, getters, execute_ref)                                                                   \
    struct className {                                                                                                 \
        getters static void execute(const Instruction &, execute_ref);                                                 \
        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);                        \
    }

// 定义指令可变, 不可变变体
#define DEF_INST(className, getters) DEF_INST_EX(className, getters, const VMState &)
#define DEF_INST_MUT(className, getters) DEF_INST_EX(className, getters, VMState &)

    // ────────────────────────────────────────────────
    // 定义指令
    // ────────────────────────────────────────────────

    DEF_INST(NOP, /* no getters */);

    // def op1
    DEF_INST(Load, OP_GET1(dst, Register) OP_GET2(value, ConstIdx));

    // def op1
    DEF_INST(ILoad, OP_GET1(dst, Register) OP_GET2(value, Integer));

    // use op1
    DEF_INST(Push, OP_GET1(src, Register));

    DEF_INST(PopN, OP_GET1(count, Integer));

    // use op1 op2 def op2
    DEF_INST(Add, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(IAdd, OP_GET1(src, Integer) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(Sub, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(ISub, OP_GET1(src, Integer) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(Mul, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(Div, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(Idiv, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(Mod, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 def op2
    DEF_INST_MUT(Mov, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 def op2
    DEF_INST_MUT(CP, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op2
    DEF_INST_MUT(DGlobal, OP_GET1(atom, Atom) OP_GET2(src, Register));

    // def op2
    DEF_INST_MUT(GGlobal, OP_GET1(atom, Atom) OP_GET2(dst, Register));

    // def op1
    DEF_INST(Global, OP_GET1(dst, Register));

    // def op1
    DEF_INST(Super, OP_GET1(dst, Register));

    // def op1
    DEF_INST(This, OP_GET1(dst, Register));

    // def op1
    DEF_INST(ToInt, OP_GET1(dst, Register));

    // def op1
    DEF_INST(ToReal, OP_GET1(dst, Register));

    // def op1
    DEF_INST(ToString, OP_GET1(dst, Register));

    // use op1 op2
    DEF_INST(ChgThis, OP_GET1(dst, Register) OP_GET2(src, Register));

    // use op1
    DEF_INST(Inv, OP_GET1(dst, Register));

    // use op1 op2 def op2
    DEF_INST(ChkInv, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op1
    DEF_INST(ChkIns, OP_GET1(dst, Register) OP_GET2(src, Register));

    // use op1
    DEF_INST_MUT(Test, OP_GET1(reg, Register));

    // use op1 op2 def op2
    DEF_INST(EQ, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(NEQ, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(LT, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(LE, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(GT, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(GE, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(AbsEQ, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(AbsNEQ, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op2
    DEF_INST(LAnd, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 def op1
    DEF_INST(LNot, OP_GET1(dst, Register));

    DEF_INST(
        Jmp,
        OP_GET1(label, Label) //
        static void target(Instruction &inst, Label label) { inst.setOp1Val(label); } //
    );

    DEF_INST(JmpE : Jmp, );
    DEF_INST(JmpNE : Jmp, );

    // use op2 def op1
    DEF_INST_MUT(Call, OP_GET1(dst, Register) OP_GET2(memberReg, Register) OP_GET3(argCount, Integer));

    // use op1 op2 def op3
    DEF_INST_MUT(GProp, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(dst, Register));

    // use op1 op2 op3
    DEF_INST(DProp, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(src, Register));

    // def op2
    DEF_INST_MUT(GThis, OP_GET1(atom, Atom) OP_GET2(dst, Register));

    // use op2
    DEF_INST_MUT(DThis, OP_GET1(atom, Atom) OP_GET2(src, Register));

    // def op2
    DEF_INST(GUpval, OP_GET1(atom, Atom) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(LOr, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BXor, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BOr, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BAnd, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BlShift, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BrShift, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 op2 def op3
    DEF_INST(BurShift, OP_GET1(src, Register) OP_GET2(dst, Register));

    // use op1 def op1
    DEF_INST(ChgSign, OP_GET1(dst, Register));

    DEF_INST(Debugger, );

    // use op1
    DEF_INST_MUT(Throw, OP_GET1(src, Register));

    // use op1
    DEF_INST_MUT(Ret, OP_GET1(retReg, Register));

    struct DumpInst {
        [[nodiscard]] static String dumpOperand(const Operand &operand);
        [[nodiscard]] static std::string autoDump(std::string_view name, const Instruction &inst, const VMState *vm);
    };
}; // namespace cial::Bytecode
