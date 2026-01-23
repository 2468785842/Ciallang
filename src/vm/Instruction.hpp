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
    O(Sub)                                                                                                             \
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
            size_t n;
            Atom atom;
        } operand;

        Type type{ Type::None };

        explicit Operand(const ConstIdx value) : operand{ .ci = value }, type(Type::ConstIndex) {}

        explicit Operand(const Register value) : operand{ .reg = value }, type(Type::Register) {}

        explicit Operand(const Label value) : operand{ .label = value }, type(Type::Label) {}

        explicit Operand(const size_t value) : operand{ .n = value }, type(Type::Number) {}

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

        [[nodiscard]] size_t operandCount() const {
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
            } else if constexpr(std::is_same_v<T, size_t>) {
                CLL_ASSERT(operand.type == Operand::Type::Number, "operand type is not Number");
                return operand.operand.n;
            } else if constexpr(std::is_same_v<T, Atom>) {
                CLL_ASSERT(operand.type == Operand::Type::Atom, "operand type is not Atom");
                return operand.operand.atom;
            } else {
                static_assert(!std::is_same_v<T, T> && "operand type is not support");
            }
            throw std::logic_error("unreachable");
        }
    };

    struct NOP {
        static void execute(const Instruction &, const VMState &) {}

        [[nodiscard]] static std::string dump(const Instruction &, const VMState *) {
            return fmt::format("{: <10}", "nop");
        }
    }; // struct NOP


    struct Load {
        static Register reg(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static ConstIdx value(const Instruction &inst) { return inst.getOperand2<ConstIdx>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Load

    struct PushReg {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct PushReg

    struct PopN {
        static size_t count(const Instruction &inst) { return inst.getOperand1<size_t>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct PushReg

    struct Add {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Add

    struct Sub {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Sub

    struct Mul {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Mul

    struct Div {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Div

    struct Idiv {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Idiv

    struct Mod {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Mod

    struct Mov {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Mov

    struct CP {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct CP

    struct DGlobal {
        static Atom atom(const Instruction &inst) { return inst.getOperand1<Atom>(); }

        static Register src(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct DGlobal

    struct GGlobal {
        static Atom atom(const Instruction &inst) { return inst.getOperand1<Atom>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct GGlobal

    struct Global {
        static Register dst(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Global

    struct Super {
        static Register dst(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Super

    struct This {
        static Register dst(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct This

    struct ToInt {
        static Register dst(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct ToInt

    struct ToReal {
        static Register dst(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Real

    struct ToString {
        static Register dst(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct String

    struct ChgThis {
        static Register dst(const Instruction &inst) { return inst.getOperand1<Register>(); }
        static Register src(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct ChgThis

    struct Inv {
        static Register dst(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Inv

    struct ChkInv {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }
        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct ChkInv

    struct ChkIns {
        static Register dst(const Instruction &inst) { return inst.getOperand1<Register>(); }
        static Register src(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct ChkIns

    struct Test {
        static Register reg(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Test

    struct EQ {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct EQ

    struct NEQ {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct NEQ

    struct LT {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct LT

    struct LE {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct LE

    struct GT {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct GT

    struct GE {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct GE

    struct AbsEQ {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct AbsEQ

    struct AbsNEQ {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct AbsNEQ

    struct LNot {

        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct LNot

    struct LAnd {

        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct LAnd

    struct Jmp {

        static Label label(const Instruction &inst) { return inst.getOperand1<Label>(); }

        static void setTarget(Instruction &inst, Label label) { inst.setOperand1(label); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Jmp

    struct JmpE {

        static Label label(const Instruction &inst) { return inst.getOperand1<Label>(); }

        static void setTarget(Instruction &inst, Label label) { inst.setOperand1(label); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct JmpE

    struct JmpNE {

        static Label label(const Instruction &inst) { return inst.getOperand1<Label>(); }

        static void setTarget(Instruction &inst, Label label) { inst.setOperand1(label); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct JmpNE

    struct Call {
        static Register dst(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register memberReg(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static size_t argCount(const Instruction &inst) { return inst.getOperand3<size_t>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Call

    struct GProp {
        static Register obj(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register memberReg(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct GProp

    struct DProp {
        static Register obj(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register memberReg(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static Register src(const Instruction &inst) { return inst.getOperand3<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct DProp                                                                                          \

    struct GThis {
        static Atom atom(const Instruction &inst) { return inst.getOperand1<Atom>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct GThis

    struct DThis {
        static Atom atom(const Instruction &inst) { return inst.getOperand1<Atom>(); }

        static Register src(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct DThis

    struct GUpval {
        static Atom atom(const Instruction &inst) { return inst.getOperand1<Atom>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct GUpval

    struct LOr {

        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct LOr

    struct BXor {

        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct BXor

    struct BOr {

        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct BOr

    struct BAnd {

        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct BAnd

    struct BlShift {

        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct BlShift

    struct BrShift {

        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct BrShift

    struct BurShift {

        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static Register dst(const Instruction &inst) { return inst.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct BurShift

    struct ChgSign {

        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct ChgSign

    struct Debugger {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Debugger

    struct Throw {
        static Register src(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Throw

    struct Ret {
        static Register retReg(const Instruction &inst) { return inst.getOperand1<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &inst, const VMState *vmState);
    }; // struct Ret

}; // namespace cial::Bytecode
