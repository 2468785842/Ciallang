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

    void Interpreter::run(Chunk* mainChunk) {
        pushCallFrame(createCallFrame(mainChunk));

        for(;;) {
            // cache hit
            size_t &pc = _currentFrame->pc;
            const auto& instList = instructions();

            if(pc >= instList.size()) break;

            if(_callStack.empty()) break;

             auto *instruction = instList[pc];
            // fmt::println("{: <6}: {}", Label{ getPC() }, instruction->dump(*this, true));
             ++pc;
            instruction->execute(*this);
        }
    }

    const TjsValue& Interpreter::reg(const Register reg) const {
        auto index = reg.index()
            + _currentFrame->registersOffset;

        CLL_ASSERT(index < _registers.size(), "index > _registers.size");

        return _registers[index];
    }

}
