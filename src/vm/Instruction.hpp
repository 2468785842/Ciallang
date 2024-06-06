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

        Jmp,
        JmpE,
        JmpNE,
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
    static size_t disassembleExtIndex(const VMChunk* chunk, const size_t ip) {
        if constexpr(MD == OpcodeMode::IEX) {
            uint8_t x = chunk->bytecodes(ip + 1);
            bool immediateMode = (x & 0x80) >> 7; // x000_0000
            uint8_t extLen = x & 0x7F;            // 0xxx_xxxx

            if(immediateMode) return extLen;

            size_t index = 0;
            for(uint8_t i = 0; i < static_cast<uint8_t>(extLen >> 4) + 1; i++) {
                index <<= 8;
                index += chunk->bytecodes(ip + i + 2);
            }
            return index;
        }
        return 0;
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
        [[nodiscard]] virtual size_t length() const = 0;

        /**
         * 反汇编不会增加 ip
         * @return 反汇编内容
         */
        virtual std::string disassemble(const Interpreter*) const = 0;

        /**
         * 执行会增加 ip, 默认已经ip+1,从ip+2开始取指令参数
         * @return 解释结果
         */
        virtual InterpretResult execute(Common::Result&, Interpreter*) const = 0;

        virtual ~Instruction() = default;
    };


    template <Common::Name NAME, size_t N>
    struct MakeInstruction : Instruction {
        InterpretResult execute(Common::Result&, Interpreter*) const override {
            LOG(FATAL)
                    << "no implement insturction function `execute`: "
                    << _name;
            std::abort();
        }

        std::string disassemble(const Interpreter* interpreter) const override {
            // if instruction is 1 length, default implement
            if constexpr(N == 1) {
                auto ss = std::ostringstream{}
                          << disassembleLine(interpreter)
                          << fmt::format("{:#06x}\t{: ^10}", interpreter->_vm.ip, name());
                return ss.str();
            } else {
                LOG(FATAL)
                        << "no implement insturction function `disassemble`"
                        << _name;
                std::abort();
            }
        }

        [[nodiscard]] const char* name() const final {
            return _name;
        }

        [[nodiscard]] size_t length() const final {
            return _length;
        }

    private:
        const char* _name = NAME.value;
        const size_t _length = N;
    };

#define DEFINE_INST(ValueName, name, offset) \
using ValueName = MakeInstruction<Common::Name(name), offset>; \
constexpr auto S_##ValueName = ValueName()

    DEFINE_INST(RetInst, "ret", 1);
    DEFINE_INST(LoadInst, "load", 1);
    DEFINE_INST(BNotInst, "bnot", 1);
    DEFINE_INST(LNotInst, "lnot", 1);

    DEFINE_INST(AddInst, "add", 1);
    DEFINE_INST(SubInst, "sub", 1);
    DEFINE_INST(MulInst, "mul", 1);
    DEFINE_INST(DivInst, "div", 1);

    DEFINE_INST(PopInst, "pop", 1);
    DEFINE_INST(PopNInst, "popn", 2);
    DEFINE_INST(PushInst, "push", 2);

    DEFINE_INST(PVoidInst, "pvoid", 1);

    DEFINE_INST(GGlobalInst, "gglobal", 2);
    DEFINE_INST(DGlobalInst, "dglobal", 2);
    DEFINE_INST(GLocalInst, "glocal", 2);
    DEFINE_INST(DLocalInst, "dlocal", 2);

    DEFINE_INST(JmpInst, "jmp", 3);
    DEFINE_INST(JmpEInst, "jmpe", 3);
    DEFINE_INST(JmpNEInst, "jmpne", 3);

#undef DEFINE_INST
    // -------------------------------------------------------------------------//
    // ------------------------- reigst instruction ----------------------------//
    // -------------------------------------------------------------------------//
    static constinit auto S_Instructions =
            frozen::make_unordered_map<Opcode, const Instruction*>({
                    { Opcode::Ret, &S_RetInst },
                    { Opcode::Load, &S_LoadInst },
                    { Opcode::BNot, &S_BNotInst },
                    { Opcode::LNot, &S_LNotInst },

                    { Opcode::Add, &S_AddInst },
                    { Opcode::Sub, &S_SubInst },
                    { Opcode::Mul, &S_MulInst },
                    { Opcode::Div, &S_DivInst },

                    { Opcode::Pop, &S_PopInst },
                    { Opcode::PopN, &S_PopNInst },
                    { Opcode::Push, &S_PushInst },

                    { Opcode::PVoid, &S_PVoidInst },

                    { Opcode::GGlobal, &S_GGlobalInst },
                    { Opcode::DGlobal, &S_DGlobalInst },
                    { Opcode::GLocal, &S_GLocalInst },
                    { Opcode::DLocal, &S_DLocalInst },

                    { Opcode::Jmp, &S_JmpInst },
                    { Opcode::JmpE, &S_JmpEInst },
                    { Opcode::JmpNE, &S_JmpNEInst },
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

    EXECUTE(RetInst) {
        interpreter->printStack();
        interpreter->printGlobal();
        return InterpretResult::OK;
    }

    EXECUTE(LoadInst) {
        auto index = getExtIndex<OpcodeMode::IEX>(interpreter);
        auto constant = interpreter->readConstant(index);
        interpreter->push(std::move(constant));
        return InterpretResult::CONTINUE;
    }

    EXECUTE(BNotInst) {
        interpreter->push(-interpreter->pop());
        return InterpretResult::CONTINUE;
    }

    EXECUTE(LNotInst) {
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
        } else {
            static_assert(
                false,
                "template error opcodes not support?: `executeBinary`"
            );
        }
        return InterpretResult::CONTINUE;
    }

    EXECUTE(AddInst) {
        return executeBinary<Opcode::Add>(interpreter);
    }

    EXECUTE(SubInst) {
        return executeBinary<Opcode::Sub>(interpreter);
    }

    EXECUTE(MulInst) {
        return executeBinary<Opcode::Mul>(interpreter);
    }

    EXECUTE(DivInst) {
        return executeBinary<Opcode::Div>(interpreter);
    }

    EXECUTE(PopInst) {
        interpreter->pop();
        return InterpretResult::CONTINUE;
    }

    EXECUTE(PopNInst) {
        interpreter->popN(interpreter->readByte());
        return InterpretResult::CONTINUE;
    }

    EXECUTE(PushInst) {
        interpreter->push(TjsValue{ static_cast<TjsInteger>(interpreter->readByte()) });
        return InterpretResult::CONTINUE;
    }

    EXECUTE(PVoidInst) {
        interpreter->pushVoid();
        return InterpretResult::CONTINUE;
    }

    // get global value
    // opcode: gglobal index,
    // read id = constant[index],
    // get value from global[id], push to stack top
    EXECUTE(GGlobalInst) {
        auto index = getExtIndex<OpcodeMode::IEX>(interpreter);

        auto identitier = interpreter->readConstant(index).asString();
        auto* value = interpreter->getGlobal(identitier);
        if(!value) {
            interpreter->error(r,
                fmt::format(
                    "Read failed Undefined variable: {}.", identitier
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
    EXECUTE(DGlobalInst) {
        auto index = getExtIndex<OpcodeMode::IEX>(interpreter);
        auto identitier = interpreter->readConstant(index).asString();
        auto& value = interpreter->peek(0);
        interpreter->putGlobal(std::move(identitier), TjsValue(value));
        return InterpretResult::CONTINUE;
    }

    // get local value
    // opcode: glocal slot,
    // read value from stack[slot], push to stack top
    EXECUTE(GLocalInst) {
        auto slot = getExtIndex<OpcodeMode::IEX>(interpreter);
        auto& value = interpreter->getStack(slot);
        interpreter->push(TjsValue{ value });
        return InterpretResult::CONTINUE;
    }

    // define(or assigment) local value
    // opcode: dlocal slot,
    // peek value from stack top, put to stack[slot]
    // !!!not pop, this value still in stack top
    EXECUTE(DLocalInst) {
        auto slot = getExtIndex<OpcodeMode::IEX>(interpreter);
        auto& value = interpreter->peek(0); // because is not change sp
        interpreter->putStack(slot, TjsValue{ value });
        return InterpretResult::CONTINUE;
    }


    template <Opcode OP>
    static InterpretResult executeJmp(Interpreter* interpreter,
                                      const std::function<void(uint16_t)>& jmpF) {
        uint16_t offset = interpreter->readByte();
        offset <<= 8;
        offset += interpreter->readByte();

        if constexpr(OP == Opcode::Jmp) {
            jmpF(offset);
        } else if constexpr(OP == Opcode::JmpE) {
            if(interpreter->peek(0).asBool())
                jmpF(offset);
        } else if constexpr(OP == Opcode::JmpNE) {
            if(!interpreter->peek(0).asBool())
                jmpF(offset);
        } else {
            static_assert(false,
                "template error opcodes not support?: `executeJmp`"
            );
        }

        return InterpretResult::CONTINUE;
    }

    EXECUTE(JmpInst) {
        executeJmp<Opcode::Jmp>(
            interpreter,
            interpreter->_vm.chunk->addBytecode);
    }

    EXECUTE(JmpEInst) {
        executeJmp<Opcode::JmpE>(
            interpreter,
            interpreter->_vm.chunk->addBytecode);
    }

    EXECUTE(JmpNEInst) {
        executeJmp<Opcode::JmpNE>(
            interpreter,
            interpreter->_vm.chunk->addBytecode);
    }

    //--------------------------------------------------------------------------//
    //----------------------------- disassemble --------------------------------//
    //--------------------------------------------------------------------------//

#define DISASSEMBLE(Inst) template <> \
    inline std::string Inst::disassemble(const Interpreter* interpreter) const

#define SIMPLE_DISASSEMBLE(Inst) \
    DISASSEMBLE(Inst) { \
         auto* chunk = interpreter->_vm.chunk; \
         auto ip = interpreter->_vm.ip; \
         auto ss = std::ostringstream{} \
                   << disassembleLine(interpreter) \
                   << fmt::format("{:#06x}\t{: ^10}", ip, name()) \
                   << fmt::format("{: ^4d}", chunk->bytecodes(ip + 1)); \
         return ss.str(); \
    }

    template <Opcode OP>
    static std::string disassembleIns(const Instruction* instruction,
                                      const Interpreter* interpreter,
                                      const VMChunk* chunk, const size_t ip) {
        // source line and opcode name
        auto ss = std::ostringstream{}
                  << disassembleLine(interpreter)
                  << fmt::format("{:#06x}\t{: ^10}", ip, instruction->name());

        switch(OP) {
            case Opcode::Jmp:
            case Opcode::JmpE:
            case Opcode::JmpNE:

                uint16_t offset = chunk->bytecodes(ip + 1);
                offset <<= 8;
                offset += chunk->bytecodes(ip + 2);

                ss << fmt::format("; {}", offset);

            default: ;
        }

        switch(OP) {
            case Opcode::Load:
            case Opcode::GGlobal:
            case Opcode::DGlobal:
                auto index = disassembleExtIndex<OpcodeMode::IEX>(chunk, ip);
                ss << fmt::format("{: ^4d}", index);

                if constexpr(OP == Opcode::Load) {
                    ss << fmt::format(" ; {}", chunk->constants(index));
                } else if constexpr(OP == Opcode::GGlobal) {
                    auto& identitier = chunk->constants(index);
                    auto val = interpreter->getGlobal(identitier.asString());

                    if(!val) {
                        ss << "(get) in global no have " << identitier.asString();
                        return ss.str();
                    }

                    ss << fmt::format(" ; {}", *val);
                } else if constexpr(OP == Opcode::DGlobal) {
                    auto& identitier = chunk->constants(index);
                    auto& value = interpreter->peek(0);

                    if(!interpreter->getGlobal(identitier.asString())) {
                        ss << "(define) in global no have " << identitier.asString();
                        return ss.str();
                    }

                    ss << fmt::format(" ; {} // put {}", identitier, value);
                }
            default: ;
        }

        return ss.str();
    }

    DISASSEMBLE(LoadInst) {
        return disassembleIns<Opcode::Load>(
            this, interpreter,
            interpreter->_vm.chunk,
            interpreter->_vm.ip
        );
    }

    SIMPLE_DISASSEMBLE(PushInst)

    SIMPLE_DISASSEMBLE(PopNInst)

    DISASSEMBLE(GGlobalInst) {
        return disassembleIns<Opcode::GGlobal>(
            this, interpreter,
            interpreter->_vm.chunk,
            interpreter->_vm.ip
        );
    }

    DISASSEMBLE(DGlobalInst) {
        return disassembleIns<Opcode::DGlobal>(
            this, interpreter,
            interpreter->_vm.chunk,
            interpreter->_vm.ip
        );
    }

    DISASSEMBLE(GLocalInst) {
        auto* chunk = interpreter->_vm.chunk;
        auto ip = interpreter->_vm.ip;

        auto slot = disassembleExtIndex<OpcodeMode::IEX>(chunk, ip);
        auto& value = interpreter->getStack(slot);

        auto ss = std::ostringstream{}
                  << disassembleLine(interpreter)
                  << fmt::format("{:#06x}\t{: ^10}", ip, name())
                  << fmt::format("{: ^4d}", slot)
                  << fmt::format(" ; stack[{}]", slot)
                  << fmt::format(" // get {}", value);
        return ss.str();
    }

    DISASSEMBLE(DLocalInst) {
        auto* chunk = interpreter->_vm.chunk;
        auto ip = interpreter->_vm.ip;

        auto& value = interpreter->peek(0);

        auto slot = disassembleExtIndex<OpcodeMode::IEX>(chunk, ip);

        auto ss = std::ostringstream{}
                  << disassembleLine(interpreter)
                  << fmt::format("{:#06x}\t{: ^10}", ip, name())
                  << fmt::format("{: ^4d}", slot)
                  << fmt::format(" ; stack[{}]", slot)
                  << fmt::format(" // put {}", value);
        return ss.str();
    }


    DISASSEMBLE(JmpInst) {
        return disassembleIns<Opcode::Jmp>(
            this, interpreter,
            interpreter->_vm.chunk,
            interpreter->_vm.ip
        );
    }

    DISASSEMBLE(JmpEInst) {
        return disassembleIns<Opcode::Jmp>(
            this, interpreter,
            interpreter->_vm.chunk,
            interpreter->_vm.ip
        );
    }

    DISASSEMBLE(JmpNEInst) {
        return disassembleIns<Opcode::Jmp>(
            this, interpreter,
            interpreter->_vm.chunk,
            interpreter->_vm.ip
        );
    }
#undef EXECUTE
#undef DISASSEMBLE
#undef SIMPLE_DISASSEMBLE
}
