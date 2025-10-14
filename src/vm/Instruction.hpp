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
#include "logging/Logger.hpp"
#include "pch.h"

#include "types/TjsValue.hpp"

#define OPCODE_ENUMS(O)                                                                                                \
    O(Load)                                                                                                            \
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
    O(Ret)

namespace Ciallang::Bytecode {
    class VMState;
}

namespace Ciallang::Bytecode::Op {

    enum class OpCode : std::uint32_t {
#define OPCODE_ENUM_CLASS(OP) OP,
        OPCODE_ENUMS(OPCODE_ENUM_CLASS)
#undef OPCODE_ENUM_CLASS
            COUNT
    };

    class Instruction;

    using ExecuteCallback = void (*)(const Instruction &, VMState &);
    using DumpCallback = std::string (*)(const Instruction &, const VMState &, bool);

    using RegisterVec = std::vector<Register>;

    struct Operand {
        enum class Type { None, Value, Register, RegisterVec, Label, SymbolIndex };
        union {
            TjsValue *value;
            Register reg;
            RegisterVec *regs;
            Label label;
            size_t symbolIndex;
        } operand{};

        Type type{ Type::None };

        explicit Operand(TjsValue &&value) : type(Type::Value) {
            this->operand.value = new TjsValue{ std::move(value) };
        }

        explicit Operand(const Register &value) : type(Type::Register) { this->operand.reg = value; }

        explicit Operand(RegisterVec *value) : type(Type::RegisterVec) { this->operand.regs = value; }

        explicit Operand(const Label &value) : type(Type::Label) { this->operand.label = value; }

        explicit Operand(const size_t value) : type(Type::SymbolIndex) { this->operand.symbolIndex = value; }

        Operand(const Operand &other) = delete;

        Operand(Operand &&other) noexcept : operand(other.operand), type(other.type) { other.type = Type::None; }

        Operand &operator=(const Operand &other) = delete;

        Operand &operator=(Operand &&other) noexcept {
            if(this != &other) {
                this->~Operand();
                new(this) Operand(std::move(other));
            }
            return *this;
        }

        ~Operand() {
            if(type == Type::Value) {
                delete operand.value;
            }
            if(type == Type::RegisterVec) {
                delete operand.regs;
            }
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
        [[nodiscard]] const T &getOperand1() const {
            return getOperand<T>(_operand1);
        }

        template <typename T>
        [[nodiscard]] const T &getOperand2() const {
            return getOperand<T>(_operand2);
        }

        template <typename T>
        [[nodiscard]] const T &getOperand3() const {
            return getOperand<T>(_operand3);
        }


        static void execute(OpCode opcode, const Instruction &itt, VMState &ipt);

        static std::string dump(OpCode opcode, const Instruction &itt, const VMState &ipt, bool info);

    private:
        std::optional<Operand> _operand1;
        std::optional<Operand> _operand2;
        std::optional<Operand> _operand3;

        template <typename T>
        [[nodiscard]] const T &getOperand(const std::optional<Operand> &operand) const {
            if constexpr(std::is_same_v<T, Register>) {
                CLL_ASSERT(operand->type == Operand::Type::Register, "operand type is not Register");
                return operand->operand.reg;
            } else if constexpr(std::is_same_v<T, RegisterVec>) {
                CLL_ASSERT(operand->type == Operand::Type::RegisterVec, "operand type is not RegisterVec");
                return *operand->operand.regs;
            } else if constexpr(std::is_same_v<T, Label>) {
                CLL_ASSERT(operand->type == Operand::Type::Label, "operand type is not Label");
                return operand->operand.label;
            } else if constexpr(std::is_same_v<T, TjsValue>) {
                CLL_ASSERT(operand->type == Operand::Type::Value, "operand type is not Value");
                return *operand->operand.value;
            } else if constexpr(std::is_same_v<T, size_t>) {
                CLL_ASSERT(operand->type == Operand::Type::SymbolIndex, "operand type is not symbolIndex");
                return operand->operand.symbolIndex;
            } else {
                return nullptr;
            }
        }
    };

    struct Load {
        static const Register &reg(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const TjsValue &value(const Instruction &itt) { return itt.getOperand2<TjsValue>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Load

    struct Add {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Add

    struct Sub {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Sub

    struct Mul {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Mul

    struct Div {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Div

    struct Mov {
        static const Register &src(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Mov

    struct DGlobal {
        static size_t symbolIndex(const Instruction &itt) { return itt.getOperand1<size_t>(); }

        static const Register &src(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct DGlobal

    struct GGlobal {
        static size_t symbolIndex(const Instruction &itt) { return itt.getOperand1<size_t>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, const VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct GGlobal

    struct Test {
        static const Register &reg(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Test

    struct EQ {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct EQ

    struct NEQ {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct NEQ

    struct LT {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct LT

    struct LE {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct LE

    struct GT {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct GT

    struct GE {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct GE

    struct AbsEQ {
        static const Register &reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const Register &dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct AbsEQ

    struct Jmp {

        static const Label &label(const Instruction &itt) { return itt.getOperand1<Label>(); }

        static void setTarget(Instruction &itt, Label label) { itt.setOperand1(label); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Jmp

    struct JmpE {

        static const Label &label(const Instruction &itt) { return itt.getOperand1<Label>(); }

        static void setTarget(Instruction &itt, Label label) { itt.setOperand1(label); }
        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct JmpE

    struct JmpNE {

        static const Label &label(const Instruction &itt) { return itt.getOperand1<Label>(); }

        static void setTarget(Instruction &itt, Label label) { itt.setOperand1(label); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct JmpNE

    struct Call {
        static const Register &dst(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static const Register &memberReg(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static const std::vector<Register> &arguments(const Instruction &itt) { return itt.getOperand3<RegisterVec>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Call

    struct Ret {
        static const Register &retReg(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static void execute(const Instruction &, VMState &);

        [[nodiscard]] static std::string dump(const Instruction &, const VMState &, bool);
    }; // struct Ret

}; // namespace Ciallang::Bytecode::Op
