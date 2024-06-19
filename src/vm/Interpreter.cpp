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

namespace Ciallang::Bytecode {

    void Interpreter::run(Chunk* mainChunk) {
        pushCallFrame(createCallFrame(mainChunk));

        for(;;) {
            // cache hit
            size_t &pc = _currentFrame->pc;
            const auto& insts = instructions();

            if(pc >= insts.size()) break;

            if(_callStack.empty()) break;

             auto *instruction = insts[pc];
            // fmt::println("{: <6}: {}", Label{ getPC() }, instruction->dump(*this, true));
             ++pc;
            instruction->execute(*this);
        }

        fmt::println("interpret over...");
    }

}
