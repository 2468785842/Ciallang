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

#include "../common/ConstExpr.hpp"
#include "Interpreter.hpp"
#include "../types/TjsString.hpp"
#include "types/TjsNativeFunction.hpp"

namespace Ciallang::VM {
    enum class OpcodeMode : uint8_t {
        IEX,
    };

    enum class Opcode : uint8_t {
        Ret, // 程序正常返回

        Load, // opcode(8) immediateMode(1) length(7) ; load const
        // if immediate
        // length is const index
        // else
        // length is ext count read for length again
        BNot, // 按位非
        LNot, // 逻辑非 -number
        Add,
        Sub,
        Mul,
        Div,

        Pop,
        PopN,
        Push,
        PVoid,

        GGlobal,
        DGlobal,
        GLocal,
        DLocal,

        Jmp,   // sign
        JmpE,  // sign
        JmpNE, // sign

        Call,

        Equals,
        NEquals,

        SEquals, //strict type and value equals
        SNEquals,
    };

    template <typename T>
    static std::vector<uint8_t> encodeIEX(const T index) {
        if(index >> 56 != 0) {
            return { 0x80 | 7,
                     static_cast<uint8_t>(index >> 56),
                     static_cast<uint8_t>(index >> 48),
                     static_cast<uint8_t>(index >> 40),
                     static_cast<uint8_t>(index >> 32),
                     static_cast<uint8_t>(index >> 24),
                     static_cast<uint8_t>(index >> 16),
                     static_cast<uint8_t>(index >> 8),
                     static_cast<uint8_t>(index),
            };
        }
        if(index >> 24 != 0) {
            return { 0x80 | 3,
                     static_cast<uint8_t>(index >> 24),
                     static_cast<uint8_t>(index >> 16),
                     static_cast<uint8_t>(index >> 8),
                     static_cast<uint8_t>(index),
            };
        }
        if(index >> 8 != 0) {
            return { 0x80 | 1,
                     static_cast<uint8_t>(index >> 8),
                     static_cast<uint8_t>(index),
            };
        }
        if((index & 0x80) != 0) {
            return { 0x80,
                     static_cast<uint8_t>(index),
            };
        }
        return { static_cast<uint8_t>(0x80 | index) };
    }


    template <OpcodeMode MD>
    static size_t getExtIndex(Interpreter* interpreter) {
        if constexpr(MD == OpcodeMode::IEX) {
            uint8_t x = interpreter->readByte();
            bool immediateMode = (x & 0x80) >> 7; // x000_0000
            uint8_t extLen = x & 0x7F;            // 0xxx_xxxx

            if(immediateMode) return extLen;

            size_t index = 0;
            for(uint8_t i = 0; i < extLen + 1; i++) {
                index <<= 8;
                index += interpreter->readByte();
            }
            return index;
        }
        return 0;
    }

    struct Instruction {
        static const Instruction* instance(Opcode opcode);

        [[nodiscard]] virtual const char* name() const = 0;
        [[nodiscard]] virtual uint8_t length() const = 0;

        std::string disassemble(const Interpreter* intprter) const {
            return disassemble(intprter, true);
        }

        /**
         * 反汇编不会增加 ip
         * @return 反汇编内容
         */
        virtual std::string disassemble(const Interpreter*, bool) const = 0;

        /**
         * 执行会增加 ip, 默认已经ip+1,从ip+2开始取指令参数
         * @return 解释结果
         */
        virtual InterpretResult execute(Common::Result&, Interpreter*) const = 0;

        virtual ~Instruction() = default;
    };

    template <Opcode OP>
    static std::string disassembleIns(const Instruction* instruction,
                                      const Interpreter* interpreter,
                                      bool stic);

    template <Common::Name NAME, size_t N, Opcode OP>
    struct MakeInstruction : Instruction {
        InterpretResult execute(Common::Result&, Interpreter*) const override {
            LOG(FATAL)
                    << "no implement insturction function `execute`: "
                    << _name;
            std::abort();
        }

        std::string disassemble(const Interpreter* interpreter, const bool stic) const override {
            // if instruction is 1 length, default implement
            if constexpr(N == 1) {
                auto ss = std::ostringstream{}
                          << interpreter->disassembleLine()
                          << fmt::format("{:#06x}\t{: ^10}",
                              interpreter->ip(),
                              name()
                          );
                return ss.str();
            } else {
                return disassembleIns<OP>(this, interpreter, stic);
            }
        }

        [[nodiscard]] const char* name() const final {
            return _name;
        }

        [[nodiscard]] uint8_t length() const final {
            return _length;
        }

    private:
        const char* _name = NAME.value;
        const size_t _length = N;
    };

#define DEFINE_INS(InsName, name, offset) \
using InsName##Ins = MakeInstruction<Common::Name(name), offset, Opcode::InsName>; \
constexpr auto S_##InsName##Ins = InsName##Ins()

    DEFINE_INS(Ret, "ret", 1);
    DEFINE_INS(Load, "load", 2); // offset is changable
    DEFINE_INS(BNot, "bnot", 1);
    DEFINE_INS(LNot, "lnot", 1);

    DEFINE_INS(Add, "add", 1);
    DEFINE_INS(Sub, "sub", 1);
    DEFINE_INS(Mul, "mul", 1);
    DEFINE_INS(Div, "div", 1);

    DEFINE_INS(Pop, "pop", 1);
    DEFINE_INS(PopN, "popn", 2);
    DEFINE_INS(Push, "push", 2);

    DEFINE_INS(PVoid, "pvoid", 1);

    DEFINE_INS(GGlobal, "gglobal", 2); // offset is changable
    DEFINE_INS(DGlobal, "dglobal", 2); // offset is changable
    DEFINE_INS(GLocal, "glocal", 2);   // offset is changable
    DEFINE_INS(DLocal, "dlocal", 2);   // offset is changable

    DEFINE_INS(Jmp, "jmp", 3);
    DEFINE_INS(JmpE, "jmpe", 3);
    DEFINE_INS(JmpNE, "jmpne", 3);

    DEFINE_INS(Call, "call", 2);

    DEFINE_INS(Equals, "equals", 1);
    DEFINE_INS(NEquals, "nequals", 1);

#undef DEFINE_INS
    // -------------------------------------------------------------------------//
    // ------------------------- reigst instruction ----------------------------//
    // -------------------------------------------------------------------------//
    static constinit auto S_Instructions =
            frozen::make_unordered_map<Opcode, const Instruction*>({
                    { Opcode::Ret, &S_RetIns },
                    { Opcode::Load, &S_LoadIns },
                    { Opcode::BNot, &S_BNotIns },
                    { Opcode::LNot, &S_LNotIns },

                    { Opcode::Add, &S_AddIns },
                    { Opcode::Sub, &S_SubIns },
                    { Opcode::Mul, &S_MulIns },
                    { Opcode::Div, &S_DivIns },

                    { Opcode::Pop, &S_PopIns },
                    { Opcode::PopN, &S_PopNIns },
                    { Opcode::Push, &S_PushIns },

                    { Opcode::PVoid, &S_PVoidIns },

                    { Opcode::GGlobal, &S_GGlobalIns },
                    { Opcode::DGlobal, &S_DGlobalIns },
                    { Opcode::GLocal, &S_GLocalIns },
                    { Opcode::DLocal, &S_DLocalIns },

                    { Opcode::Jmp, &S_JmpIns },
                    { Opcode::JmpE, &S_JmpEIns },
                    { Opcode::JmpNE, &S_JmpNEIns },

                    { Opcode::Call, &S_CallIns },

                    { Opcode::Equals, &S_EqualsIns },
                    { Opcode::NEquals, &S_NEqualsIns }
            });

    inline const Instruction* Instruction::instance(const Opcode opcode) {
        const auto it = S_Instructions.find(opcode);
        if(it == S_Instructions.end()) {
            LOG(FATAL)
                    << "not find instruction!! opcode enum: 0x"
                    << std::hex << static_cast<size_t>(opcode);
        }
        return it->second;
    }

    //--------------------------------------------------------------------------//
    //------------------------------- execute ----------------------------------//
    //--------------------------------------------------------------------------//
#define EXECUTE(Inst) template <> \
inline InterpretResult Inst::execute(Common::Result &r, Interpreter* interpreter) const

    EXECUTE(RetIns) {

        if(interpreter->isCallFrameTop()) {
            return InterpretResult::OK;
        }

        // define default parameter value, set return value to slot
        // default parameter maybe exprNode, we compile exprNode to chunk
        // and when call function we will execute all chunk(with exprNode)
        if(interpreter->isSubCallFrame()) {
            interpreter->resovleParameter(interpreter->pop());
            interpreter->callFrameRet();
            return InterpretResult::CONTINUE;
        }

        auto returnValue = interpreter->pop();

        interpreter->callFrameRet();

        // return value
        interpreter->push(std::move(returnValue));

        return InterpretResult::CONTINUE;
    }

    EXECUTE(LoadIns) {
        auto index = getExtIndex<OpcodeMode::IEX>(interpreter);
        const auto& constant = interpreter->readConstant(index);
        interpreter->push(TjsValue{ constant });
        return InterpretResult::CONTINUE;
    }

    EXECUTE(BNotIns) {
        interpreter->push(-interpreter->pop());
        return InterpretResult::CONTINUE;
    }

    EXECUTE(LNotIns) {
        // interpreter->push(!interpreter->pop());
        return InterpretResult::CONTINUE;
    }

    template <Opcode OP>
    static InterpretResult executeBinary(Interpreter* interpreter) {
        auto b = interpreter->pop();
        auto a = interpreter->pop();

        if constexpr(OP == Opcode::Add) {
            interpreter->push(a + b);
        } else if constexpr(OP == Opcode::Sub) {
            interpreter->push(a - b);
        } else if constexpr(OP == Opcode::Mul) {
            interpreter->push(a * b);
        } else if constexpr(OP == Opcode::Div) {
            interpreter->push(a / b);
        } else if constexpr(OP == Opcode::Equals) {
            interpreter->push(TjsValue{ static_cast<TjsInteger>(a == b) });
        } else if constexpr(OP == Opcode::NEquals) {
            interpreter->push(TjsValue{ static_cast<TjsInteger>(a != b) });
        } else {
            static_assert(
                false,
                "template error opcodes not support?: `executeBinary`"
            );
        }
        return InterpretResult::CONTINUE;
    }

    EXECUTE(AddIns) {
        return executeBinary<Opcode::Add>(interpreter);
    }

    EXECUTE(SubIns) {
        return executeBinary<Opcode::Sub>(interpreter);
    }

    EXECUTE(MulIns) {
        return executeBinary<Opcode::Mul>(interpreter);
    }

    EXECUTE(DivIns) {
        return executeBinary<Opcode::Div>(interpreter);
    }

    EXECUTE(EqualsIns) {
        return executeBinary<Opcode::Equals>(interpreter);
    }

    EXECUTE(NEqualsIns) {
        return executeBinary<Opcode::NEquals>(interpreter);
    }

    EXECUTE(PopIns) {
        interpreter->pop();
        return InterpretResult::CONTINUE;
    }

    EXECUTE(PopNIns) {
        interpreter->popN(interpreter->readByte());
        return InterpretResult::CONTINUE;
    }

    EXECUTE(PushIns) {
        interpreter->push(TjsValue{ static_cast<TjsInteger>(interpreter->readByte()) });
        return InterpretResult::CONTINUE;
    }

    EXECUTE(PVoidIns) {
        interpreter->pushVoid();
        return InterpretResult::CONTINUE;
    }

    // get global value
    // opcode: gglobal index,
    // read id = constant[index],
    // get value from global[id], push to stack top
    EXECUTE(GGlobalIns) {
        auto index = getExtIndex<OpcodeMode::IEX>(interpreter);

        auto identitier = interpreter->readConstant(index).asString();
        auto* value = interpreter->getGlobal(*identitier);
        if(!value) {
            interpreter->error(r,
                fmt::format(
                    "Read failed Undefined variable: {}.", *identitier
                )
            );
            return InterpretResult::RUNTIME_ERROR;
        }
        interpreter->push(TjsValue{ *value });
        return InterpretResult::CONTINUE;
    }

    // define(or assigment) global value
    // opcode: dglobal index,
    // read id = constant[index],
    // peek value from stack top, put to global[id]
    // !!!not pop, this value still in stack top
    EXECUTE(DGlobalIns) {
        auto index = getExtIndex<OpcodeMode::IEX>(interpreter);
        auto identitier = *interpreter->readConstant(index).asString();
        const auto& value = interpreter->peek(0);
        interpreter->putGlobal(std::move(identitier), TjsValue{ value });
        return InterpretResult::CONTINUE;
    }

    // get local value
    // opcode: glocal slot,
    // read value from stack[slot], push to stack top
    EXECUTE(GLocalIns) {
        auto slot = getExtIndex<OpcodeMode::IEX>(interpreter);
        const auto& value = interpreter->getStack(slot);
        interpreter->push(TjsValue{ value });
        return InterpretResult::CONTINUE;
    }

    // define(or assigment) local value
    // opcode: dlocal slot,
    // peek value from stack top, put to stack[slot]
    // !!!not pop, this value still in stack top
    EXECUTE(DLocalIns) {
        auto slot = getExtIndex<OpcodeMode::IEX>(interpreter);
        auto value = TjsValue{ interpreter->peek(0) }; // because is not change sp
        interpreter->putStack(slot, std::move(value));
        return InterpretResult::CONTINUE;
    }


    template <Opcode OP>
    static InterpretResult executeJmp(Interpreter* interpreter) {
        uint16_t offset = interpreter->readByte();
        offset <<= 8;
        offset += interpreter->readByte();

        if constexpr(OP == Opcode::Jmp) {
            interpreter->ip(static_cast<int16_t>(offset));
        } else if constexpr(OP == Opcode::JmpE) {
            if(interpreter->peek(0).asBool())
                interpreter->ip(static_cast<int16_t>(offset));
        } else if constexpr(OP == Opcode::JmpNE) {
            if(!interpreter->peek(0).asBool())
                interpreter->ip(static_cast<int16_t>(offset));
        } else {
            static_assert(false,
                "template error opcodes not support?: `executeJmp`"
            );
        }

        return InterpretResult::CONTINUE;
    }

    EXECUTE(JmpIns) {
        return executeJmp<Opcode::Jmp>(interpreter);
    }

    EXECUTE(JmpEIns) {
        return executeJmp<Opcode::JmpE>(interpreter);
    }

    EXECUTE(JmpNEIns) {
        return executeJmp<Opcode::JmpNE>(interpreter);
    }

    EXECUTE(CallIns) {
        const auto value = interpreter->pop();

        auto argumentsCount = interpreter->pop().asInteger();


        auto* obj = value.asObject();

        // much more, pop
        if(argumentsCount > obj->arity()) {
            interpreter->popN(argumentsCount - obj->arity());
        }

        // much less, push void
        if(argumentsCount < obj->arity()) {
            while(argumentsCount < obj->arity()) {
                interpreter->pushVoid();
                ++argumentsCount;
            }
        }

        if(obj->isNative()) {
            auto* nativeFun = dynamic_cast<TjsNativeFunction*>(obj);
            auto* args = interpreter->popArgs(argumentsCount);
            interpreter->push(nativeFun->callProc(args));

            return InterpretResult::CONTINUE;
        }

        auto* fun = dynamic_cast<TjsFunction*>(obj);

        if(fun == nullptr) {
            r.error("is not function call error");
            return InterpretResult::RUNTIME_ERROR;
        }

        interpreter->call(fun);

        return InterpretResult::CONTINUE;
    }


#undef EXECUTE
    //--------------------------------------------------------------------------//
    //----------------------------- disassemble --------------------------------//
    //--------------------------------------------------------------------------//

    template
    <
        OpcodeMode MD
    >
    static size_t disassembleExtIndex(const VMChunk& chunk, const size_t ip) {
        if constexpr(MD == OpcodeMode::IEX) {
            uint8_t x = chunk.bytecodes(ip + 1);
            bool immediateMode = (x & 0x80) >> 7; // x000_0000
            uint8_t extLen = x & 0x7F;            // 0xxx_xxxx

            if(immediateMode) return extLen;

            size_t index = 0;
            for(uint8_t i = 0; i < static_cast<uint8_t>(extLen >> 4) + 1; i++) {
                index <<= 8;
                index += chunk.bytecodes(ip + i + 2);
            }
            return index;
        }
        return 0;
    }

    template
    <
        Opcode OP
    >
    static std::string disassembleIns(const Instruction* instruction,
                                      const Interpreter* interpreter, const bool stic) {
        const auto& chunk = interpreter->chunk();
        const size_t ip = interpreter->ip();
        // source line and opcode name
        auto ss = std::ostringstream{}
                  << interpreter->disassembleLine()
                  << fmt::format("{:#06x}\t{: ^10}", ip, instruction->name());
        if constexpr(OP == Opcode::Call) {
            ss << fmt::format(" ; {}", interpreter->peek(0));
        } else if constexpr(OP == Opcode::Pop || OP == Opcode::Push) {
            ss << fmt::format("{: ^4d}", chunk->bytecodes(ip + 1));
        } else if constexpr(OP == Opcode::Jmp
                            || OP == Opcode::JmpE
                            || OP == Opcode::JmpNE) {
            uint16_t offset = chunk->bytecodes(ip + 1);
            offset <<= 8;
            offset |= chunk->bytecodes(ip + 2);

            ss << fmt::format("{:#06x} ; abs {:#06x}",
                static_cast<int16_t>(offset),
                ip + static_cast<int16_t>(offset));
        } else if constexpr(OP == Opcode::Load
                            || OP == Opcode::GGlobal
                            || OP == Opcode::DGlobal
                            || OP == Opcode::GLocal
                            || OP == Opcode::DLocal) {
            auto index = disassembleExtIndex<OpcodeMode::IEX>(*chunk, ip);
            ss << fmt::format("{: ^4d}", index);

            if(stic) return ss.str();

            if constexpr(OP == Opcode::Load) {
                ss << fmt::format(" ; {}", chunk->constants(index));
            } else if constexpr(OP == Opcode::GGlobal) {
                auto& identitier = chunk->constants(index);
                const auto* val = interpreter->getGlobal(*identitier.asString());

                if(!val) {
                    ss << "(get) in global no have " << identitier;
                    return ss.str();
                }

                ss << fmt::format(" ; {}", *val);
            } else if constexpr(OP == Opcode::DGlobal) {
                const auto& identitier = chunk->constants(index);
                const auto& value = interpreter->peek(0);

                if(!interpreter->getGlobal(*identitier.asString())) {
                    ss << "(declare) " << identitier;
                    return ss.str();
                }

                ss << fmt::format(" ; {} // put {}", identitier, value);
            } else if constexpr(OP == Opcode::GLocal) {
                const auto& value = interpreter->getStack(index);

                ss << fmt::format(" ; stack[{}]", index)
                        << fmt::format(" // get {}", value);
            } else /* if constexpr(OP == Opcode::DLocal) */ {
                const auto& value = interpreter->peek(0);
                ss << fmt::format(" ; stack[{}]", index)
                        << fmt::format(" // put {}", value);
            }
        }

        return ss.str();
    }
}
