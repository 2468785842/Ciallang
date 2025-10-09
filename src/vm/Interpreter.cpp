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

        for(;;) {
            // cache hit
            size_t &pc = _currentFrame->pc;
            const auto &instList = instructions();

            if(pc >= instList.size())
                break;

            if(_stackTop == 0)
                break;

            const auto *instruction = instList[pc];
            ++pc;
            instruction->execute(*this);
        }
    }

    void Interpreter::reg(const Register reg, TjsValue value) const {
        _currentFrame->getReg(reg.index()) = std::move(value);
    }

    TjsValue Interpreter::reg(const Register reg) {
        const auto index = reg.index();
        CLL_ASSERT(index < _currentFrame->chunk->getRegisterCount(), "index > registers count");
        return _currentFrame->getReg(index);
    }

    const TjsValue &Interpreter::reg(const Register reg) const {
        const auto index = reg.index();
        CLL_ASSERT(index < _currentFrame->chunk->getRegisterCount(), "index > registers count");
        return _currentFrame->getReg(index);
    }
} // namespace Ciallang::Bytecode
