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

#include <glog/logging.h>

#include "VMChunk.hpp"
#include "Instruction.hpp"

namespace Ciallang::VM {
    #define READ_BYTE() (*_vm.ip++)
    #define READ_CONSTANT() (_vm.chunk->constants()[READ_BYTE()])

    Interpreter::Interpreter(VMChunk* chunk)
        : _vm{ chunk, chunk->bytecodes(), false } {
    }

    using enum InterpretResult;
    InterpretResult Interpreter::run() {
        for(;;) {
            auto opcode = static_cast<Opcodes>(READ_BYTE());
            if(opcode == Opcodes::Ret) return OK;

            const auto* inst = Instruction::instance(opcode);
            LOG(INFO) << inst->disassemble(this, opcode);
            inst->execute(this);
        }
    }

    TjsValue Interpreter::readConstant() {
        return READ_CONSTANT();
    }

}
