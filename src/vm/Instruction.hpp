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
    O(PushReg)                                                                                                         \
    O(PopN)                                                                                                            \
    O(CP)                                                                                                              \
    O(Add)                                                                                                             \
    O(AddImm)                                                                                                          \
    O(Sub)                                                                                                             \
    O(SubImm)                                                                                                          \
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

    enum class OpCode : std::uint16_t {
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
        union {
            Register reg;
            Label label;
            ConstIdx ci;
            Integer num;
            Atom atom;
        } operand;

        Type type{ Type::None };

        explicit Operand(const ConstIdx value) : operand{ .ci = value }, type(Type::ConstIndex) {}

        explicit Operand(const Register value) : operand{ .reg = value }, type(Type::Register) {}

        explicit Operand(const Label value) : operand{ .label = value }, type(Type::Label) {}

        explicit Operand(const Integer value) : operand{ .num = value }, type(Type::Number) {}

        explicit Operand(const Atom value) : operand{ .atom = value }, type(Type::Atom) {}

        Operand(const Operand &other) = delete;

        Operand &operator=(const Operand &other) = delete;

        Operand(Operand &&other) noexcept : operand(other.operand), type(other.type) { other.type = Type::None; }

        Operand &operator=(Operand &&other) noexcept {
            if(this != &other) {
                this->~Operand();
                new(this) Operand(std::move(other));
            }
            return *this;
        }
    };

    class Instruction {
    public:
        const OpCode opcode;

        explicit Instruction(const OpCode opcode) : opcode(opcode) {}
        explicit Instruction(const OpCode opcode, Operand &&operand) : opcode(opcode), _operand1(std::move(operand)) {}
        explicit Instruction(const OpCode opcode, Operand &&operand1, Operand &&operand2) :
            opcode(opcode), _operand1(std::move(operand1)), _operand2(std::move(operand2)) {}
        explicit Instruction(const OpCode opcode, Operand &&operand1, Operand &&operand2, Operand &&operand3) :
            opcode(opcode), _operand1(std::move(operand1)), _operand2(std::move(operand2)),
            _operand3(std::move(operand3)) {}

        Instruction(const Instruction &other) = delete;

        Instruction(Instruction &&other) noexcept : opcode(other.opcode) {
            if(other._operand1)
                _operand1 = std::move(*other._operand1);
            if(other._operand2)
                _operand2 = std::move(*other._operand2);
            if(other._operand3)
                _operand3 = std::move(*other._operand3);
        }

        Instruction &operator=(const Operand &other) = delete;

        Instruction &operator=(Instruction &&other) = delete;

        template <typename T>
        void setOperand1(T &&v) {
            _operand1 = Operand{ std::forward<T>(v) };
        }

        template <typename T>
        void setOperand2(T &&v) {
            _operand2 = Operand{ std::forward<T>(v) };
        }

        template <typename T>
        void setOperand3(T &&v) {
            _operand3 = Operand{ std::forward<T>(v) };
        }

        template <typename T>
        [[nodiscard]] constexpr T getOperand1() const {
            CLL_ASSERT(_operand1, "operand1 is empty");
            return getOperand<T>(*_operand1);
        }

        template <typename T>
        [[nodiscard]] constexpr T getOperand2() const {
            CLL_ASSERT(_operand2, "operand2 is empty");
            return getOperand<T>(*_operand2);
        }

        template <typename T>
        [[nodiscard]] constexpr T getOperand3() const {
            CLL_ASSERT(_operand3, "operand3 is empty");
            return getOperand<T>(*_operand3);
        }

        [[nodiscard]] const Operand::Type &getOperand1Type() const { return _operand1->type; }

        [[nodiscard]] const Operand::Type &getOperand2Type() const { return _operand2->type; }

        [[nodiscard]] const Operand::Type &getOperand3Type() const { return _operand3->type; }

        [[nodiscard]] Integer operandCount() const {
            return static_cast<int>(_operand1.has_value()) + static_cast<int>(_operand2.has_value()) +
                static_cast<int>(_operand3.has_value());
        }

        static void execute(const Instruction &inst, VMState &vmState);

        static std::string dump(const Instruction &inst, const VMState *vmState = nullptr);

    private:
        std::optional<Operand> _operand1;
        std::optional<Operand> _operand2;
        std::optional<Operand> _operand3;

        template <typename T>
        [[nodiscard]] constexpr T getOperand(const Operand &operand) const {
            if constexpr(std::is_same_v<T, Register>) {
                CLL_ASSERT(operand.type == Operand::Type::Register, "operand type is not Register");
                return operand.operand.reg;
            } else if constexpr(std::is_same_v<T, Label>) {
                CLL_ASSERT(operand.type == Operand::Type::Label, "operand type is not Label");
                return operand.operand.label;
            } else if constexpr(std::is_same_v<T, ConstIdx>) {
                CLL_ASSERT(operand.type == Operand::Type::ConstIndex, "operand type is not ConstIdx");
                return operand.operand.ci;
            } else if constexpr(std::is_same_v<T, Integer>) {
                CLL_ASSERT(operand.type == Operand::Type::Number, "operand type is not Number");
                return operand.operand.num;
            } else if constexpr(std::is_same_v<T, Atom>) {
                CLL_ASSERT(operand.type == Operand::Type::Atom, "operand type is not Atom");
                return operand.operand.atom;
            } else {
                static_assert(!std::is_same_v<T, T> && "operand type is not support");
            }
            throw std::logic_error("unreachable");
        }
    };

// 操作码
#define OP_GET1(name, type)                                                                                            \
    static type name(const Instruction &inst) { return inst.getOperand1<type>(); }
#define OP_GET2(name, type)                                                                                            \
    static type name(const Instruction &inst) { return inst.getOperand2<type>(); }
#define OP_GET3(name, type)                                                                                            \
    static type name(const Instruction &inst) { return inst.getOperand3<type>(); }

// 定义指令
#define DEF_INST_EX(className, getters, execute_ref, dump_ref)                                                         \
    struct className {                                                                                                 \
        getters static void execute(const Instruction &, execute_ref);                                                 \
        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);                        \
    }

// 定义指令可变, 不可变变体
#define DEF_INST(className, getters) DEF_INST_EX(className, getters, const VMState &, const VMState *)
#define DEF_INST_MUT(className, getters) DEF_INST_EX(className, getters, VMState &, VMState &)

    // ────────────────────────────────────────────────
    // 定义指令
    // ────────────────────────────────────────────────

    DEF_INST(NOP, /* no getters */);

    DEF_INST(Load, OP_GET1(reg, Register) OP_GET2(value, ConstIdx));

    DEF_INST(PushReg, OP_GET1(src, Register));

    DEF_INST(PopN, OP_GET1(count, Integer));

    DEF_INST(Add, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(AddImm, OP_GET1(src, Integer) OP_GET2(dst, Register));

    DEF_INST(Sub, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(SubImm, OP_GET1(src, Integer) OP_GET2(dst, Register));

    DEF_INST(Mul, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(Div, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(Idiv, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(Mod, OP_GET1(src, Register) OP_GET2(dst, Register));

    // 需要修改 VMState 的用 MUT 版本
    DEF_INST_MUT(Mov, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST_MUT(CP, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST_MUT(DGlobal, OP_GET1(atom, Atom) OP_GET2(src, Register));

    DEF_INST_MUT(GGlobal, OP_GET1(atom, Atom) OP_GET2(dst, Register));

    DEF_INST(Global, OP_GET1(dst, Register));

    DEF_INST(Super, OP_GET1(dst, Register));

    DEF_INST(This, OP_GET1(dst, Register));

    DEF_INST(ToInt, OP_GET1(dst, Register));

    DEF_INST(ToReal, OP_GET1(dst, Register));

    DEF_INST(ToString, OP_GET1(dst, Register));

    DEF_INST(ChgThis, OP_GET1(dst, Register) OP_GET2(src, Register));

    DEF_INST(Inv, OP_GET1(dst, Register));

    DEF_INST(ChkInv, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(ChkIns, OP_GET1(dst, Register) OP_GET2(src, Register));

    DEF_INST_MUT(Test, OP_GET1(reg, Register));

    DEF_INST(EQ, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(NEQ, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(LT, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(LE, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(GT, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(GE, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(AbsEQ, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(AbsNEQ, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(LNot, OP_GET1(src, Register));

    DEF_INST(LAnd, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(Jmp, OP_GET1(label, Label));

    DEF_INST(JmpE, OP_GET1(label, Label));

    DEF_INST(JmpNE, OP_GET1(label, Label));

    DEF_INST_MUT(Call, OP_GET1(dst, Register) OP_GET2(memberReg, Register) OP_GET3(argCount, Integer));

    DEF_INST_MUT(GProp, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(dst, Register));

    DEF_INST(DProp, OP_GET1(obj, Register) OP_GET2(memberReg, Register) OP_GET3(src, Register));

    DEF_INST_MUT(GThis, OP_GET1(atom, Atom) OP_GET2(dst, Register));

    DEF_INST_MUT(DThis, OP_GET1(atom, Atom) OP_GET2(src, Register));

    DEF_INST(GUpval, OP_GET1(atom, Atom) OP_GET2(dst, Register));

    DEF_INST(LOr, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(BXor, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(BOr, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(BAnd, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(BlShift, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(BrShift, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(BurShift, OP_GET1(src, Register) OP_GET2(dst, Register));

    DEF_INST(ChgSign, OP_GET1(src, Register));

    DEF_INST(Debugger, OP_GET1(src, Register));

    DEF_INST_MUT(Throw, OP_GET1(src, Register));

    DEF_INST_MUT(Ret, OP_GET1(retReg, Register));
}; // namespace cial::Bytecode
