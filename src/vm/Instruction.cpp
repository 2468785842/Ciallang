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

#include "Instruction.hpp"

#include "Interpreter.hpp"
#include "logging/Logger.hpp"
#include "types/TjsFunction.hpp"
#include "types/TjsNativeFunction.hpp"

namespace Ciallang::Bytecode::Op {

    void Load::execute(const Instruction &itt, const Interpreter &ipt) { ipt.reg(reg(itt), value(itt)); }

    std::string Load::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4} {: <4}", "load", reg(itt), value(itt));
    }

    void Add::execute(const Instruction &itt, const Interpreter &ipt) {
        ipt.reg(dst(itt), ipt.reg(reg1(itt)) + ipt.reg(reg2(itt)));
    }

    std::string Add::dump(const Instruction &itt, const Interpreter &ipt, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "add", reg1(itt), reg2(itt), dst(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, reg1(itt), ipt.reg(reg1(itt)), reg2(itt),
                           ipt.reg(reg2(itt)));
    }

    void Sub::execute(const Instruction &itt, const Interpreter &ipt) {
        ipt.reg(dst(itt), ipt.reg(reg1(itt)) - ipt.reg(reg2(itt)));
    }

    std::string Sub::dump(const Instruction &itt, const Interpreter &ipt, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "sub", reg1(itt), reg2(itt), dst(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, reg1, ipt.reg(reg1(itt)), reg2(itt),
                           ipt.reg(reg2(itt)));
    }

    void Mul::execute(const Instruction &itt, Interpreter &ipt) {
        ipt.reg(dst(itt), ipt.reg(reg1(itt)) * ipt.reg(reg2(itt)));
    }

    std::string Mul::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "mul", reg1(itt), reg2(itt), dst(itt));
    }

    void Div::execute(const Instruction &itt, Interpreter &ipt) {
        ipt.reg(dst(itt), ipt.reg(reg1(itt)) / ipt.reg(reg2(itt)));
    }

    std::string Div::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "div", reg1(itt), reg2(itt), dst(itt));
    }

    void Mov::execute(const Instruction &itt, Interpreter &ipt) {
        const auto &srcVal = ipt.reg(src(itt));
        ipt.reg(dst(itt), srcVal);
    }

    std::string Mov::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4} {: <4}", "mov", src(itt), dst(itt));
    }

    void DGlobal::execute(const Instruction &itt, Interpreter &ipt) {
        const auto &value = ipt.reg(src(itt));
        ipt.global(symbolIndex(itt), TjsValue{ value });
    }

    std::string DGlobal::dump(const Instruction &itt, const Interpreter &ipt, const bool info) {
        const auto &symbol = fmt::format("\"{}\"", ipt.getSymbol(symbolIndex(itt)));
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "dglobal", src(itt), symbol);

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, src(itt), ipt.reg(src(itt)));
    }

    void GGlobal::execute(const Instruction &itt, const Interpreter &ipt) {
        const auto &value = ipt.global(symbolIndex(itt));
        ipt.reg(dst(itt), value);
    }

    std::string GGlobal::dump(const Instruction &itt, const Interpreter &ipt, const bool info) {
        const auto &symbol = fmt::format("\"{}\"", ipt.getSymbol(symbolIndex(itt)));
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "gglobal", symbol, dst(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, symbol, ipt.global(symbolIndex(itt)));
    }

    void Test::execute(const Instruction &itt, Interpreter &ipt) {
        if(static_cast<const Interpreter &>(ipt).reg(reg(itt)).toBool()) {
            ipt.setZF(true);
        }
    }

    std::string Test::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4}", "test", reg(itt));
    }

    void EQ::execute(const Instruction &itt, Interpreter &ipt) {
        const auto value1 = ipt.reg(reg1(itt));
        const auto value2 = ipt.reg(reg2(itt));
        const bool result = value1 == value2;
        ipt.reg(dst(itt), TjsValue{ static_cast<TjsInteger>(result) });
        ipt.setZF(result);
    }

    std::string EQ::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "eq", reg1(itt), reg2(itt), dst(itt));
    }

    void NEQ::execute(const Instruction &itt, Interpreter &ipt) {
        const auto value1 = ipt.reg(reg1(itt));
        const auto value2 = ipt.reg(reg2(itt));
        const bool result = value1 != value2;
        ipt.reg(dst(itt), TjsValue{ static_cast<TjsInteger>(result) });
        ipt.setZF(result);
    }

    std::string NEQ::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4}, {: <4}, {: <4}", "neq", reg1(itt), reg2(itt), dst(itt));
    }

    void LT::execute(const Instruction &itt, Interpreter &ipt) {
        const auto &kIpt = static_cast<const Interpreter &>(ipt);
        const auto &value1 = kIpt.reg(reg1(itt));
        const auto &value2 = kIpt.reg(reg2(itt));
        const bool result = value1 < value2;
        ipt.reg(dst(itt), TjsValue{ static_cast<TjsInteger>(result) });
        ipt.setZF(result);
    }

    std::string LT::dump(const Instruction &itt, const Interpreter &ipt, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "lt", reg1(itt), reg2(itt), dst(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, reg1(itt), ipt.reg(reg1(itt)), reg2(itt),
                           ipt.reg(reg2(itt)));
    }

    void LE::execute(const Instruction &itt, Interpreter &ipt) {
        const auto value1 = ipt.reg(reg1(itt));
        const auto value2 = ipt.reg(reg2(itt));
        const bool result = value1 <= value2;
        ipt.reg(dst(itt), TjsValue{ static_cast<TjsInteger>(result) });
        ipt.setZF(result);
    }

    std::string LE::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "le", reg1(itt), reg2(itt), dst(itt));
    }

    void GT::execute(const Instruction &itt, Interpreter &ipt) {
        const auto value1 = ipt.reg(reg1(itt));
        const auto value2 = ipt.reg(reg2(itt));
        const bool result = value1 > value2;
        ipt.reg(dst(itt), TjsValue{ static_cast<TjsInteger>(result) });
        ipt.setZF(result);
    }

    std::string GT::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "ge", reg1(itt), reg2(itt), dst(itt));
    }

    void GE::execute(const Instruction &itt, Interpreter &ipt) {
        const auto value1 = ipt.reg(reg1(itt));
        const auto value2 = ipt.reg(reg2(itt));
        const bool result = value1 >= value2;
        ipt.reg(dst(itt), TjsValue{ static_cast<TjsInteger>(result) });
        ipt.setZF(result);
    }

    std::string GE::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "ge", reg1(itt), reg2(itt), dst(itt));
    }

    void AbsEQ::execute(const Instruction &, Interpreter &) {
        // TODO:
        assert(false);
    }

    std::string AbsEQ::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "abseq", reg1(itt), reg2(itt), dst(itt));
    }

    void Jmp::execute(const Instruction &itt, Interpreter &ipt) { ipt.setPC(label(itt)); }

    std::string Jmp::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {: <4}", "jmp", label(itt));
    }

    void JmpE::execute(const Instruction &itt, Interpreter &ipt) {
        if(ipt.getZF()) {
            ipt.setPC(label(itt));
        }
    }

    std::string JmpE::dump(const Instruction &itt, const Interpreter &ipt, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4}", "jmpe", label(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; ZF = {}", insDump, ipt.getZF());
    }

    void JmpNE::execute(const Instruction &itt, Interpreter &ipt) {
        if(!ipt.getZF()) {
            ipt.setPC(label(itt));
        }
    }

    std::string JmpNE::dump(const Instruction &itt, const Interpreter &ipt, const bool info) {
        auto insDump = fmt::format("{: <10} {: <4}", "jmpne", label(itt));

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; ZF = {}", insDump, ipt.getZF());
    }

    void Call::execute(const Instruction &itt, Interpreter &ipt) {
        // call const ref is Faster
        const auto &object = static_cast<const Interpreter &>(ipt).reg(memberReg(itt));
        CLL_ASSERT(object.isObject(), "memberReg is not object");

        if(!object.toObject()->isNative()) {
            const auto fun = dynamic_cast<TjsFunction *>(object.toObject());
            if(!fun) {
                throw std::runtime_error{ "not function a type" };
            }
            CallFrame callFrame = ipt.createCallFrame(fun->chunk(), dst(itt));

            const auto &args = arguments(itt);
            for(std::uint32_t i = 0; i < args.size(); i++) {
                // copy
                callFrame.getReg(i) = ipt.reg(args[i]);
            }

            ipt.pushCallFrame(std::move(callFrame));
            return;
        }

        const auto args = arguments(itt);
        const auto values = std::make_unique<TjsValue[]>(args.size());
        for(std::uint32_t i = 0; i < args.size(); i++) {
            values[i] = ipt.reg(args[i]);
        }

        auto value = dynamic_cast<TjsNativeFunction *>(object.toObject())->callProc(values.get());
        ipt.reg(dst(itt), std::move(value));
    }

    std::string Call::dump(const Instruction &itt, const Interpreter &, bool) {
        std::stringstream ss{};
        ss << fmt::format("{: <10} {: <4} {: <4}", "call", memberReg(itt), dst(itt));
        for(const auto &argument : arguments(itt)) {
            ss << fmt::format("{: <4}", argument);
        }
        return ss.str();
    }

    void Ret::execute(const Instruction &itt, Interpreter &ipt) {
        auto value = ipt.reg(retReg(itt));
        const auto &frame = ipt.popCallFrame();
        CLL_ASSERT(frame.ret.has_value(), "frame.ret val is empty");
        ipt.reg(frame.ret.value(), std::move(value));
    }

    std::string Ret::dump(const Instruction &itt, const Interpreter &, bool) {
        return fmt::format("{: <10} {}", "ret", retReg(itt));
    }

    void Instruction::execute(const OpCode opcode, const Instruction &itt, Interpreter &ipt) {
#define HANDLE_OPCODE(OP)                                                                                              \
    case OpCode::OP:                                                                                                   \
        OP::execute(itt, ipt);                                                                                         \
        break;
        switch(opcode) {
            OPCODE_ENUMS(HANDLE_OPCODE)
            default:;
        }
    }

    std::string Instruction::dump(const OpCode opcode, const Instruction &itt, const Interpreter &ipt,
                                  const bool info) {
#define DUMP_OPCODE(OP)                                                                                                \
    case OpCode::OP:                                                                                                   \
        return OP::dump(itt, ipt, info);
        switch(opcode) {
            OPCODE_ENUMS(DUMP_OPCODE)
            default:;
        }
        return "";
    }


} // namespace Ciallang::Bytecode::Op
