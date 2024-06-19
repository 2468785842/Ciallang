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
#include "pch.h"
#include "Register.hpp"

#include "types/TjsValue.hpp"

namespace Ciallang::Bytecode {
    class Interpreter;
}

namespace Ciallang::Bytecode::Op {
    class Instruction {
    public:
        virtual void execute(Interpreter&) const = 0;

        virtual std::string dump(const Interpreter&, bool) const = 0;

        virtual ~Instruction() = default;
    };

    class Load final : public Instruction {
    public:
        explicit Load(
            const Register reg, TjsValue&& value
        ) : _reg(reg), _value(std::move(value)) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

        const TjsValue& value() const { return _value; }

    private:
        const Register _reg;
        const TjsValue _value;
    };

    class Add final : public Instruction {
    public:
        explicit Add(
            const Register reg1,
            const Register reg2,
            const Register dst): _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&,bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };

    class Sub final : public Instruction {
    public:
        explicit Sub(
            const Register reg1,
            const Register reg2,
            const Register dst) : _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&,bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };

    class Mul final : public Instruction {
    public:
        explicit Mul(
            const Register reg1,
            const Register reg2,
            const Register dst) : _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };

    class Div final : public Instruction {
    public:
        explicit Div(
            const Register reg1,
            const Register reg2,
            const Register dst) :
            _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };


    class Mov final : public Instruction {
    public:
        explicit Mov(
            const Register src,
            const Register dst
        ) : _src(src), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _src;
        const Register _dst;
    };

    class DGlobal final : public Instruction {
    public:
        explicit DGlobal(
            std::string&& identifier,
            const Register src
        ) : _src(src), _identifier(std::move(identifier)) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _src;
        const std::string _identifier;
    };

    class GGlobal final : public Instruction {
    public:
        explicit GGlobal(
            std::string&& identifier,
            const Register dst
        ) : _identifier(identifier), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const std::string _identifier;
        const Register _dst;
    };

    class Test final : public Instruction {
    public:
        explicit Test(
            const Register reg
        ): _reg(reg) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _reg;
    };

    class EQ final : public Instruction {
    public:
        explicit EQ(
            const Register reg1,
            const Register reg2,
            const Register dst
        ) : _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };

    class NEQ final : public Instruction {
    public:
        explicit NEQ(
            const Register reg1,
            const Register reg2,
            const Register dst
        ) : _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };

    class LT final : public Instruction {
    public:
        explicit LT(
            const Register reg1,
            const Register reg2,
            const Register dst
        ) : _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };

    class LE final : public Instruction {
    public:
        explicit LE(
            const Register reg1,
            const Register reg2,
            const Register dst
        ) : _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };

    class GT final : public Instruction {
    public:
        explicit GT(
            const Register reg1,
            const Register reg2,
            const Register dst
        ) : _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };

    class GE final : public Instruction {
    public:
        explicit GE(
            const Register reg1,
            const Register reg2,
            const Register dst
        ) : _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };

    // ===
    class AbsEQ final : public Instruction {
    public:
        explicit AbsEQ(
            const Register reg1,
            const Register reg2,
            const Register dst
        ): _reg1(reg1), _reg2(reg2), _dst(dst) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _reg1;
        const Register _reg2;
        const Register _dst;
    };

    class Jmp final : public Instruction {
    public:
        explicit Jmp() = default;

        void setTarget(Label label) { _label.emplace(label); }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        std::optional<Label> _label;
    };

    class JmpE final : public Instruction {
    public:
        explicit JmpE() = default;

        void setTarget(Label label) { _label.emplace(label); }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        std::optional<Label> _label;
    };

    class JmpNE final : public Instruction {
    public:
        explicit JmpNE() = default;

        void setTarget(Label label) { _label.emplace(label); }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        std::optional<Label> _label;
    };

    class Call final : public Instruction {
    public:
        explicit Call(
            const Register dst,
            const Register memberReg,
            std::vector<Register>&& arguments
        ) : _dst(dst), _memberReg(memberReg), _arguments(arguments) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        const Register _dst;
        const Register _memberReg;
        const std::vector<Register> _arguments;
    };

    class Ret final : public Instruction {
    public :
        explicit Ret(const Register retReg): _retReg(retReg) {
        }

        void execute(Interpreter&) const override;

        std::string dump(const Interpreter&, bool) const override;

    private:
        std::optional<Register> _retReg{};
    };
}
