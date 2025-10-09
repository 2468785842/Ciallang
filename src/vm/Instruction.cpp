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
    void Load::execute(Interpreter &interpreter) const { interpreter.reg(_reg, _value); }

    std::string Load::dump(const Interpreter &, bool) const {
        return fmt::format("{: <10} {: <4} {: <4}", "load", _reg, _value);
    }

    void Add::execute(Interpreter &interpreter) const {
        interpreter.reg(_dst, interpreter.reg(_reg1) + interpreter.reg(_reg2));
    }

    std::string Add::dump(const Interpreter &interpreter, const bool info) const {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "add", _reg1, _reg2, _dst);

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, _reg1, interpreter.reg(_reg1), _reg2,
                           interpreter.reg(_reg2));
    }

    void Sub::execute(Interpreter &interpreter) const {
        interpreter.reg(_dst, interpreter.reg(_reg1) - interpreter.reg(_reg2));
    }

    std::string Sub::dump(const Interpreter &interpreter, const bool info) const {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "sub", _reg1, _reg2, _dst);

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, _reg1, interpreter.reg(_reg1), _reg2,
                           interpreter.reg(_reg2));
    }

    void Mul::execute(Interpreter &interpreter) const {
        interpreter.reg(_dst, interpreter.reg(_reg1) * interpreter.reg(_reg2));
    }

    std::string Mul::dump(const Interpreter &, bool) const {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "mul", _reg1, _reg2, _dst);
    }

    void Div::execute(Interpreter &interpreter) const {
        interpreter.reg(_dst, interpreter.reg(_reg1) / interpreter.reg(_reg2));
    }

    std::string Div::dump(const Interpreter &, bool) const {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "div", _reg1, _reg2, _dst);
    }

    void Mov::execute(Interpreter &interpreter) const {
        const auto &srcVal = interpreter.reg(_src);
        interpreter.reg(_dst, srcVal);
    }

    std::string Mov::dump(const Interpreter &, bool) const {
        return fmt::format("{: <10} {: <4} {: <4}", "mov", _src, _dst);
    }

    void DGlobal::execute(Interpreter &interpreter) const {
        const auto &value = interpreter.reg(_src);
        interpreter.global(_symbolIndex, TjsValue{ value });
    }

    std::string DGlobal::dump(const Interpreter &interpreter, const bool info) const {
        const auto &symbol = fmt::format("\"{}\"", interpreter.getSymbol(_symbolIndex));
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "dglobal", _src, symbol);

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, _src, interpreter.reg(_src));
    }

    void GGlobal::execute(Interpreter &interpreter) const {
        auto &value = interpreter.global(_symbolIndex);
        interpreter.reg(_dst, value);
    }

    std::string GGlobal::dump(const Interpreter &interpreter, const bool info) const {
        const auto &symbol = fmt::format("\"{}\"", interpreter.getSymbol(_symbolIndex));
        auto insDump = fmt::format("{: <10} {: <4} {: <4}", "gglobal", symbol, _dst);

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}", insDump, symbol, interpreter.global(_symbolIndex));
    }

    void Test::execute(Interpreter &interpreter) const {
        if(interpreter.reg(_reg).toBool()) {
            interpreter.setZF(true);
        }
    }

    std::string Test::dump(const Interpreter &, bool) const { return fmt::format("{: <10} {: <4}", "test", _reg); }

    void EQ::execute(Interpreter &interpreter) const {
        const auto value1 = interpreter.reg(_reg1);
        const auto value2 = interpreter.reg(_reg2);
        const bool result = value1 == value2;
        interpreter.reg(_dst, TjsValue{ static_cast<TjsInteger>(result) });
        interpreter.setZF(result);
    }

    std::string EQ::dump(const Interpreter &, bool) const {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "eq", _reg1, _reg2, _dst);
    }

    void NEQ::execute(Interpreter &interpreter) const {
        const auto value1 = interpreter.reg(_reg1);
        const auto value2 = interpreter.reg(_reg2);
        const bool result = value1 != value2;
        interpreter.reg(_dst, TjsValue{ static_cast<TjsInteger>(result) });
        interpreter.setZF(result);
    }

    std::string NEQ::dump(const Interpreter &, bool) const {
        return fmt::format("{: <10} {: <4}, {: <4}, {: <4}", "neq", _reg1, _reg2, _dst);
    }

    void LT::execute(Interpreter &interpreter) const {
        const auto value1 = interpreter.reg(_reg1);
        const auto value2 = interpreter.reg(_reg2);
        const bool result = value1 < value2;
        interpreter.reg(_dst, TjsValue{ static_cast<TjsInteger>(result) });
        interpreter.setZF(result);
    }

    std::string LT::dump(const Interpreter &interpreter, const bool info) const {
        auto insDump = fmt::format("{: <10} {: <4} {: <4} {: <4}", "lt", _reg1, _reg2, _dst);

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; {} = {}, {} = {}", insDump, _reg1, interpreter.reg(_reg1), _reg2,
                           interpreter.reg(_reg2));
    }

    void LE::execute(Interpreter &interpreter) const {
        const auto value1 = interpreter.reg(_reg1);
        const auto value2 = interpreter.reg(_reg2);
        const bool result = value1 <= value2;
        interpreter.reg(_dst, TjsValue{ static_cast<TjsInteger>(result) });
        interpreter.setZF(result);
    }

    std::string LE::dump(const Interpreter &, bool) const {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "le", _reg1, _reg2, _dst);
    }

    void GT::execute(Interpreter &interpreter) const {
        const auto value1 = interpreter.reg(_reg1);
        const auto value2 = interpreter.reg(_reg2);
        const bool result = value1 > value2;
        interpreter.reg(_dst, TjsValue{ static_cast<TjsInteger>(result) });
        interpreter.setZF(result);
    }

    std::string GT::dump(const Interpreter &, bool) const {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "ge", _reg1, _reg2, _dst);
    }

    void GE::execute(Interpreter &interpreter) const {
        const auto value1 = interpreter.reg(_reg1);
        const auto value2 = interpreter.reg(_reg2);
        const bool result = value1 >= value2;
        interpreter.reg(_dst, TjsValue{ static_cast<TjsInteger>(result) });
        interpreter.setZF(result);
    }

    std::string GE::dump(const Interpreter &, bool) const {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "ge", _reg1, _reg2, _dst);
    }

    void AbsEQ::execute(Interpreter &) const {
        // TODO:
        assert(false);
    }

    std::string AbsEQ::dump(const Interpreter &, bool) const {
        return fmt::format("{: <10} {: <4} {: <4} {: <4}", "abseq", _reg1, _reg2, _dst);
    }

    void Jmp::execute(Interpreter &interpreter) const {
        CLL_ASSERT(_label.has_value(), "label is empty");
        interpreter.setPC(_label.value());
    }

    std::string Jmp::dump(const Interpreter &interpreter, bool info) const {
        CLL_ASSERT(_label.has_value(), "label is empty");
        return fmt::format("{: <10} {: <4}", "jmp", _label.value());
    }

    void JmpE::execute(Interpreter &interpreter) const {
        if(interpreter.getZF()) {
            CLL_ASSERT(_label.has_value(), "label is empty");
            interpreter.setPC(_label.value());
        }
    }

    std::string JmpE::dump(const Interpreter &interpreter, const bool info) const {
        CLL_ASSERT(_label.has_value(), "label is empty");
        auto insDump = fmt::format("{: <10} {: <4}", "jmpe", _label.value());

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; ZF = {}", insDump, interpreter.getZF());
    }

    void JmpNE::execute(Interpreter &interpreter) const {
        if(!interpreter.getZF()) {
            CLL_ASSERT(_label.has_value(), "label is empty");
            interpreter.setPC(_label.value());
        }
    }

    std::string JmpNE::dump(const Interpreter &interpreter, const bool info) const {
        CLL_ASSERT(_label.has_value(), "label is empty");
        auto insDump = fmt::format("{: <10} {: <4}", "jmpne", _label.value());

        if(!info)
            return insDump;

        return fmt::format("{: <30} ; ZF = {}", insDump, interpreter.getZF());
    }

    void Call::execute(Interpreter &interpreter) const {
        const auto &object = interpreter.reg(_memberReg);
        CLL_ASSERT(object.isObject(), "memberReg is not object");

        if(!object.toObject()->isNative()) {
            const auto fun = dynamic_cast<TjsFunction *>(object.toObject());
            if(!fun) {
                throw std::runtime_error{ "unsupported function type(not function type)" };
            }
            CallFrame callFrame = interpreter.createCallFrame(fun->chunk(), _dst);

            for(std::uint32_t i = 0; i < _arguments.size(); i++) {
                // copy
                callFrame.getReg(i) = interpreter.reg(_arguments[i]);
            }

            interpreter.pushCallFrame(std::move(callFrame));
            return;
        }

        const auto values = std::make_unique<TjsValue[]>(_arguments.size());
        for(std::uint32_t i = 0; i < _arguments.size(); i++) {
            values[i] = interpreter.reg(_arguments[i]);
        }
        auto value = dynamic_cast<TjsNativeFunction *>(object.toObject())->callProc(values.get());
        interpreter.reg(_dst, std::move(value));
    }

    std::string Call::dump(const Interpreter &, bool) const {
        std::stringstream ss{};
        ss << fmt::format("{: <10} {: <4} {: <4}", "call", _memberReg, _dst);
        for(auto &argument : _arguments) {
            ss << fmt::format("{: <4}", argument);
        }
        return ss.str();
    }

    void Ret::execute(Interpreter &interpreter) const {
        // copy
        auto value = interpreter.reg(_retReg.value());
        const auto frame = interpreter.popCallFrame();
        CLL_ASSERT(frame.ret.has_value(), "frame.ret val is empty");
        interpreter.reg(frame.ret.value(), std::move(value));
    }

    std::string Ret::dump(const Interpreter &, bool) const { return fmt::format("{: <10} {}", "ret", _retReg.value()); }
} // namespace Ciallang::Bytecode::Op
