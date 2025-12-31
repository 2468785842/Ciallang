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

#include "types/Value.hpp"

namespace Cial::Bytecode {
    class VMState;
}

namespace Cial::Bytecode::Op {

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
    O(Mov)                                                                                                             \
    O(DGlobal)                                                                                                         \
    O(GGlobal)                                                                                                         \
    O(Test)                                                                                                            \
    O(EQ)                                                                                                              \
    O(NEQ)                                                                                                             \
    O(LT)                                                                                                              \
    O(LE)                                                                                                              \
    O(GT)                                                                                                              \
    O(GE)                                                                                                              \
    O(AbsEQ)                                                                                                           \
    O(Jmp)                                                                                                             \
    O(JmpE)                                                                                                            \
    O(JmpNE)                                                                                                           \
    O(Call)                                                                                                            \
    O(GProp)                                                                                                           \
    O(SProp)                                                                                                           \
    O(GUpval)                                                                                                          \
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

        static void execute(const Instruction &itt, VMState &vmState);

        static std::string dump(const Instruction &itt, const VMState &vmState, bool info);

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

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool) {
            return fmt::format("{: <10}", "nop");
        }
    }; // struct NOP


    struct Load {
        static Register reg(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static ConstIdx value(const Instruction &itt) { return itt.getOperand2<ConstIdx>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Load

    struct PushReg {
        static Register src(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct PushReg

    struct PopN {
        static size_t count(const Instruction &itt) { return itt.getOperand1<size_t>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct PushReg

    struct CP {
        static Register dst(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register src(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct CP

    struct Add {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Add

    struct Sub {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Sub

    struct Mul {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Mul

    struct Div {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Div

    struct Mov {
        static Register src(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Mov

    struct DGlobal {
        static Atom atom(const Instruction &itt) { return itt.getOperand1<Atom>(); }

        static Register src(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct DGlobal

    struct GGlobal {
        static Atom atom(const Instruction &itt) { return itt.getOperand1<Atom>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct GGlobal

    struct Test {
        static Register reg(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Test

    struct EQ {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct EQ

    struct NEQ {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct NEQ

    struct LT {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct LT

    struct LE {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct LE

    struct GT {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct GT

    struct GE {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct GE

    struct AbsEQ {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct AbsEQ

    struct Jmp {

        static Label label(const Instruction &itt) { return itt.getOperand1<Label>(); }

        static void setTarget(Instruction &itt, Label label) { itt.setOperand1(label); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Jmp

    struct JmpE {

        static Label label(const Instruction &itt) { return itt.getOperand1<Label>(); }

        static void setTarget(Instruction &itt, Label label) { itt.setOperand1(label); }
        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct JmpE

    struct JmpNE {

        static Label label(const Instruction &itt) { return itt.getOperand1<Label>(); }

        static void setTarget(Instruction &itt, Label label) { itt.setOperand1(label); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct JmpNE

    struct Call {
        static Register dst(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register memberReg(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static size_t argCount(const Instruction &itt) { return itt.getOperand3<size_t>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Call

    struct GProp {
        static Register obj(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct GProp

    struct SProp {
        static Register obj(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const String *name(const Instruction &itt, const VMState &vmState);

        static Value value(const Instruction &itt, const VMState &vmState);

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct SProp

    struct GUpval {
        static Atom atom(const Instruction &itt) { return itt.getOperand1<Atom>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct GUpval

    struct Ret {
        static Register retReg(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Ret

}; // namespace Cial::Bytecode::Op
