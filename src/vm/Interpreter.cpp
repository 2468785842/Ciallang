/*
 * Copyright (c) 2024/5/8 上午8:08
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
#include "Interpreter.hpp"

#include "Chunk.hpp"
#include "Instruction.hpp"
#include "logging/Logger.hpp"

namespace Ciallang::Bytecode {

    void Interpreter::run(const Chunk *mainChunk) {
        pushCallFrame(createCallFrame(mainChunk));
// #define CLL_COMPUTED_GOTO
#ifdef CLL_COMPUTED_GOTO
        // 标签数组
        static void *labels[] = {
#define HANDLE_OPCODE(OP) &&label_##OP,
            OPCODE_ENUMS(HANDLE_OPCODE)
#undef HANDLE_OPCODE
        };

        goto label_Dispatch;

    label_Dispatch: {
        const auto &instList = instructions();
        const Op::Instruction &instruction = instList[_currentFrame->pc];
        if(_currentFrame->pc >= instList.size() || _stackTop == 0)
            return;

        goto *labels[static_cast<size_t>(instruction.opcode)];
    }

#define HANDLE_OPCODE(OP)                                                                                              \
    label_##OP : {                                                                                                     \
        const Op::Instruction &instruction = instructions()[_currentFrame->pc];                                        \
        ++_currentFrame->pc;                                                                                           \
        Op::OP::execute(instruction, *this);                                                                           \
        goto label_Dispatch;                                                                                           \
    }

        OPCODE_ENUMS(HANDLE_OPCODE)
#undef HANDLE_OPCODE
#else
        for(;;) {
            // cache hit
            size_t &pc = _currentFrame->pc;
            const auto &instList = instructions();

            if(pc >= instList.size())
                break;

            if(_stackTop == 0)
                break;

            const auto &instruction = instList[pc];
            ++pc;
            Op::Instruction::execute(instruction.opcode, instruction, *this);
        }
#endif
    }

    void Interpreter::reg(const Register &reg, const TjsValue &value) const {
        _currentFrame->getReg(reg.index()) = TjsValue{ value };
    }

    void Interpreter::reg(const Register &reg, TjsValue &&value) const {
        _currentFrame->getReg(reg.index()) = std::move(value);
    }

    TjsValue Interpreter::reg(const Register reg) { return _currentFrame->getReg(reg.index()); }

    const TjsValue &Interpreter::reg(const Register reg) const { return _currentFrame->getReg(reg.index()); }
} // namespace Ciallang::Bytecode
