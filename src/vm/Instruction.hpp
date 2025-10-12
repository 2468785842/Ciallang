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

namespace Ciallang::Bytecode {
    class Interpreter;
}

namespace Ciallang::Bytecode::Op {

#define OPCODE_ENUMS(O) \
    O(Load) \
    O(Add) \
    O(Sub) \
    O(Mul) \
    O(Div) \
    O(Mov) \
    O(DGlobal) \
    O(GGlobal) \
    O(Test) \
    O(EQ) \
    O(NEQ) \
    O(LT) \
    O(LE) \
    O(GT) \
    O(GE) \
    O(AbsEQ) \
    O(Jmp) \
    O(JmpE) \
    O(JmpNE) \
    O(Call) \
    O(Ret)

#define OPCODE_ENUM_CLASS(OP) OP,
    enum class OpCode : std::uint8_t {
        OPCODE_ENUMS(OPCODE_ENUM_CLASS)
        COUNT
    };
#undef OPCODE_ENUM_CLASS

    class Instruction;

    using ExecuteCallback = void (*)(const Instruction &, Interpreter &);
    using DumpCallback = std::string (*)(const Instruction &, const Interpreter &, bool);

    using RegisterVec = std::vector<Register> *;

    struct Operand {
        enum class Type {
            None,
            Value,
            Register,
            RegisterVec,
            Label,
            SymbolIndex
        };
        union {
            TjsValue *value;
            Register reg;
            RegisterVec regs;
            Label label;
            size_t symbolIndex;
        } operand{};

        Type type{ Type::None };

        explicit Operand(TjsValue &&value) : type(Type::Value) {
            this->operand.value = new TjsValue{std::move(value)};
        }

        explicit Operand(const Register &value) : type(Type::Register) { this->operand.reg = value; }

        explicit Operand(RegisterVec value) : type(Type::RegisterVec) { this->operand.regs = value; }

        explicit Operand(const Label &value) : type(Type::Label) { this->operand.label = value; }

        explicit Operand(const size_t value) : type(Type::SymbolIndex) { this->operand.symbolIndex = value; }

        Operand(const Operand &other) = delete;

        Operand(Operand &&other) noexcept {
            type = other.type;
            operand = other.operand;
            other.type = Type::None;
        }

        Operand &operator=(const Operand &other) = delete;

        Operand &operator=(Operand &&other) noexcept {
            if (this == &other) return *this;
            this->~Operand();
            this->operand = other.operand;
            this->type = other.type;
            other.type = Type::None;
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

        struct Handler {
            ExecuteCallback exec;
            DumpCallback dump;
        };

        explicit Instruction(const OpCode opcode) : _opcode(opcode) {}
        explicit Instruction(const OpCode opcode, Operand &&operand) : _opcode(opcode), _operand1(std::move(operand)) {}
        explicit Instruction(const OpCode opcode, Operand &&operand1, Operand &&operand2) :
            _opcode(opcode), _operand1(std::move(operand1)), _operand2(std::move(operand2)) {}
        explicit Instruction(const OpCode opcode, Operand &&operand1, Operand &&operand2, Operand &&operand3) :
            _opcode(opcode), _operand1(std::move(operand1)), _operand2(std::move(operand2)), _operand3(std::move(operand3)) {}

        [[nodiscard]] OpCode getOpcode() const { return _opcode; }

        template <typename T>
        void setOperand1(T &&v) {
            _operand1 = Operand{std::forward<T>(v)};
        }

        template <typename T>
        void setOperand2(T &&v) {
            _operand2 = Operand{std::forward<T>(v)};
        }

        template <typename T>
        void setOperand3(T &&v) {
            _operand3 = Operand{std::forward<T>(v)};
        }

        template<typename T>
        [[nodiscard]] const T &getOperand1() const {
            return getOperand<T>(_operand1);
        }

        template<typename T>
        [[nodiscard]] const T &getOperand2() const {
            return getOperand<T>(_operand2);
        }

        template<typename T>
        [[nodiscard]] const T &getOperand3() const {
            return getOperand<T>(_operand3);
        }


        static void execute(OpCode opcode, const Instruction &itt, Interpreter & ipt);

        static std::string dump(OpCode opcode, const Instruction & itt, const Interpreter & ipt, bool info);

    private:
        const OpCode _opcode;
        std::optional<Operand> _operand1;
        std::optional<Operand> _operand2;
        std::optional<Operand> _operand3;

        template<typename T>
        [[nodiscard]] const T &getOperand(const std::optional<Operand> &operand) const {
            if constexpr (std::is_same_v<T, Register>) {
                CLL_ASSERT(operand->type == Operand::Type::Register, "operand type is not Register");
                return operand->operand.reg;
            }else if constexpr (std::is_same_v<T, RegisterVec>) {
                CLL_ASSERT(operand->type == Operand::Type::RegisterVec, "operand type is not RegisterVec");
                return operand->operand.regs;
            }else if constexpr (std::is_same_v<T, Label>) {
                CLL_ASSERT(operand->type == Operand::Type::Label, "operand type is not Label");
                return operand->operand.label;
            }else if constexpr (std::is_same_v<T, TjsValue>) {
                CLL_ASSERT(operand->type == Operand::Type::Value, "operand type is not Value");
                return *operand->operand.value;
            }else if constexpr (std::is_same_v<T, size_t>) {
                CLL_ASSERT(operand->type == Operand::Type::SymbolIndex, "operand type is not symbolIndex");
                return operand->operand.symbolIndex;
            }
            static_assert(std::is_same_v<T, T>, "type unsupported");
        }
    };

    namespace Load {
        static Register reg(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static TjsValue value(const Instruction &itt) { return itt.getOperand2<TjsValue>(); }

        static void execute(const Instruction &, const Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace Load

    namespace Add {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace Add

    namespace Sub {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, const Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace Sub

    namespace Mul {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace Mul

    namespace Div {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace Div

    namespace Mov {
        static Register src(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace Mov

    namespace DGlobal {
        static size_t symbolIndex(const Instruction &itt) { return itt.getOperand1<size_t>(); }

        static Register src(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace DGlobal

    namespace GGlobal {
        static size_t symbolIndex(const Instruction &itt) { return itt.getOperand1<size_t>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace GGlobal

    namespace Test {
        static Register reg(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace Test

    namespace EQ {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace EQ

    namespace NEQ {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace NEQ

    namespace LT {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace LT

    namespace LE {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace LE

    namespace GT {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace GT

    namespace GE {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace GE

    namespace AbsEQ {
        static Register reg1(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register reg2(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static Register dst(const Instruction &itt) { return itt.getOperand3<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace AbsEQ

    namespace Jmp {

        static Label label(const Instruction &itt) { return itt.getOperand1<Label>(); }

        static void setTarget(Instruction &itt, Label label) { itt.setOperand1(label); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace Jmp

    namespace JmpE {

        static Label label(const Instruction &itt) { return itt.getOperand1<Label>(); }

        static void setTarget(Instruction &itt, Label label) { itt.setOperand1(label); }
        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace JmpE

    namespace JmpNE {

        static Label label(const Instruction &itt) { return itt.getOperand1<Label>(); }

        static void setTarget(Instruction &itt, Label label) { itt.setOperand1(label); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace JmpNE

    namespace Call {
        static Register dst(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static Register memberReg(const Instruction &itt) { return itt.getOperand2<Register>(); }

        static std::vector<Register> *arguments(const Instruction &itt) {
            return itt.getOperand3<std::vector<Register> *>();
        }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace Call

    namespace Ret {
        static Register retReg(const Instruction &itt) { return itt.getOperand1<Register>(); }

        static void execute(const Instruction &, Interpreter &);

        [[nodiscard]] static std::string dump(const Instruction &, const Interpreter &, bool);
    } // namespace Ret

} // namespace Ciallang::Bytecode::Op
