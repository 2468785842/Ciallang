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

#include "Interpreter.hpp"
#include "Rlc.hpp"
#include "VMChunk.hpp"

namespace Ciallang::VM {
    enum class Opcodes : uint8_t {
        Ret,   // 程序正常返回
        Const, // 加载常量
        LNot,  // 逻辑非 -number
        Add,
        Sub,
        Div,
        Mul,
    };

    struct Instruction {
        static const Instruction* instance(Opcodes opcode);

        [[nodiscard]] virtual const char* name() const = 0;
        [[nodiscard]] virtual size_t length() const = 0;

        /**
         * 反汇编不会增加 ip
         * @param interpreter 解释器
         * @return 反汇编内容
         */
        std::string disassemble(const Interpreter* interpreter) const {
            return disassemble(
                interpreter->_vm.ip,
                interpreter->_vm.chunk->constants(),
                interpreter->_vm.chunk->rlc(),
                interpreter->_vm.ip - interpreter->_vm.chunk->bytecodes()
            );
        }

        /**
         * 执行会增加 ip, 默认已经ip+1,从ip+2开始取指令参数
         * @param interpreter 解释器
         * @return 解释结果
         */
        virtual InterpretResult execute(Interpreter* interpreter) const = 0;

        virtual std::string disassemble(const uint8_t* bytecodes,
                                        const TjsValue* constants,
                                        const Rlc* rlc, size_t offset) const = 0;

        virtual ~Instruction() = default;
    };

    template <size_t N>
    struct InstName {
        char value[N]{};

        constexpr explicit InstName(const char (&value)[N]) {
            std::copy_n(value, N, this->value);
        }
    };

    template <InstName NAME, size_t N>
    struct MakeInstruction : Instruction {
        InterpretResult execute(Interpreter* interpreter) const override {
            LOG(FATAL) << "no implement insturction function `execute`";
            std::abort();
        }

        std::string disassemble(const uint8_t* bytecodes,
                                const TjsValue* constants,
                                const Rlc* rlc, size_t ip) const override {
            LOG(FATAL) << "no implement insturction function `disassemble`";
            std::abort();
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
using ValueName = MakeInstruction<InstName(name), offset>; \
constexpr auto S_##ValueName = ValueName()

    DEFINE_INST(RetInst, "ret", 1);
    DEFINE_INST(ConstInst, "const", 2);
    DEFINE_INST(LNotInst, "lnot", 1);

    DEFINE_INST(AddInst, "add", 1);
    DEFINE_INST(SubInst, "sub", 1);
    DEFINE_INST(DivInst, "div", 1);
    DEFINE_INST(MulInst, "mul", 1);

#undef DEFINE_INST
    // -------------------------------------------------------------------------//
    // ------------------------- reigst instruction ----------------------------//
    // -------------------------------------------------------------------------//
    static constinit auto S_Instructions =
            frozen::make_unordered_map<Opcodes, const Instruction*>({
                    { Opcodes::Ret, &S_RetInst },
                    { Opcodes::Const, &S_ConstInst },
                    { Opcodes::LNot, &S_LNotInst },

                    { Opcodes::Add, &S_AddInst },
                    { Opcodes::Sub, &S_SubInst },
                    { Opcodes::Div, &S_DivInst },
                    { Opcodes::Mul, &S_MulInst },
            });

    inline const Instruction* Instruction::instance(const Opcodes opcode) {
        const auto it = S_Instructions.find(opcode);
        if(it == S_Instructions.end()) {
            LOG(FATAL)
                    << "not find instruction!! opcode enum: "
                    << std::hex << static_cast<size_t>(opcode);
        }
        return it->second;
    }

    //--------------------------------------------------------------------------//
    //------------------------------- execute ----------------------------------//
    //--------------------------------------------------------------------------//
#define EXECUTE(Inst) template <> \
inline InterpretResult Inst::execute(Interpreter* interpreter) const

    EXECUTE(RetInst) {
        interpreter->printStack();
        return InterpretResult::OK;
    }

    EXECUTE(ConstInst) {
        auto constant = interpreter->readConstant();
        interpreter->push(std::move(constant));
        return InterpretResult::CONTINUE;
    }

    EXECUTE(LNotInst) {
        interpreter->push(-interpreter->pop());
        return InterpretResult::CONTINUE;
    }

#define EXECUTE_BINARY_OP(Inst, op) \
    EXECUTE(Inst) { \
        auto b = interpreter->pop(); \
        auto a = interpreter->pop(); \
        interpreter->push(a op b); \
        return InterpretResult::CONTINUE; \
    }

    // EXECUTE_BINARY_OP(AddInst, +)

#undef EXECUTE
    //--------------------------------------------------------------------------//
    //----------------------------- disassemble --------------------------------//
    //--------------------------------------------------------------------------//

#define START (std::ostringstream{}
#define PRINT_NAME fmt::format("{:#04x}\t{:<6}", offset, name())
#define PRINT_LINE fmt::format("{:>5}", rlc->firstAppear(offset) ? std::to_string(rlc->find(offset) + 1) : "~")
#define PRINT_CONSTANT fmt::format(" // {};", constants[bytecodes[offset + 1]])
#define END ' ').str()
#define DISASSEMBLE(Inst) template <> \
    inline std::string Inst::disassemble( \
        const uint8_t* bytecodes, \
        const TjsValue* constants, \
        const Rlc* rlc, size_t offset) const


    DISASSEMBLE(RetInst) {
        return START << PRINT_NAME << PRINT_LINE << END;
    }

    DISASSEMBLE(ConstInst) {
        return START << PRINT_NAME << PRINT_LINE << PRINT_CONSTANT << END;
    }

    DISASSEMBLE(LNotInst) {
        return START << PRINT_NAME << PRINT_LINE << END;
    }

#undef START
#undef PRINT_NAME
#undef PRINT_LINE
#undef PRINT_CONSTANT
#undef END
#undef DISASSEMBLE
}
