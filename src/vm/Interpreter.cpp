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

#include "VMChunk.hpp"
#include "Instruction.hpp"

namespace Ciallang::VM {
    #define READ_BYTE() (*_vm.ip++)
    #define READ_CONSTANT() (_vm.chunk->constants()[READ_BYTE()])

    Interpreter::Interpreter(VMChunk* chunk)
        : _vm{ chunk, chunk->bytecodes() } {
    }

    InterpretResult Interpreter::run() {
        for(;;) {
            auto opcode = static_cast<Opcodes>(*_vm.ip);

            const auto* inst = Instruction::instance(opcode);

            LOG(INFO) << inst->disassemble(this);

            ++_vm.ip;

            auto result = inst->execute(this);
            if(result != InterpretResult::CONTINUE) {
                return result;
            }

        }
    }

    TjsValue Interpreter::readConstant() {
        return READ_CONSTANT();
    }

    inline void Interpreter::push(TjsValue&& value) {
        *_vm.sp++ = std::move(value);
    }

    inline TjsValue Interpreter::pop() {
        return *--_vm.sp;
    }

    void Interpreter::printStack() {
        fmt::print(
            "+{0:─^{2}}+\n"
            "|{1: ^{2}}|\n"
            "+{0:─^{2}}+\n", "", "stack", 20);
        for(TjsValue* slot = _vm.stack; slot < _vm.sp; slot++) {
            fmt::print("|{0: ^{1}}|\n", *slot, 20);
        }
        fmt::print("+{0:─^{1}}+\n", "", 20);
    }
}
