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
    Interpreter::Interpreter(Common::SourceFile& sourceFile, VMChunk* chunk)
        : _sourceFile(sourceFile), _vm{ .chunk = chunk } {
    }

    InterpretResult Interpreter::run(Common::Result& r) {
        for(;;) {
            auto opcode = static_cast<Opcodes>(_vm.chunk->bytecodes(_vm.ip));

            const auto* inst = Instruction::instance(opcode);

            LOG(INFO) << inst->disassemble(this);

            ++_vm.ip;

            auto result = inst->execute(r, this);
            if(result != InterpretResult::CONTINUE) {
                LOG(INFO) << "VM return";
                return result;
            }
        }
    }

    uint8_t Interpreter::readByte() {
        return _vm.chunk->bytecodes()[_vm.ip++];
    }

    TjsValue Interpreter::readConstant() {
        return _vm.chunk->constants()[readByte()];
    }

    TjsValue& Interpreter::peek(const size_t distance) const {
        return _vm.sp[distance - 1];
    }

    void Interpreter::pushVoid() {
        ++_vm.sp;
    }

    void Interpreter::push(TjsValue&& value) {
        *_vm.sp++ = std::move(value);
    }

    TjsValue& Interpreter::popN(const size_t n) {
        _vm.sp -= n;
        return *_vm.sp;
    }

    TjsValue& Interpreter::pop() {
        return *--_vm.sp;
    }

    TjsValue& Interpreter::getStack(const size_t slot) {
        return _vm.stack[slot];
    }

    void Interpreter::putStack(const size_t slot, TjsValue&& value) {
        _vm.stack[slot] = std::move(value);
    }

    void Interpreter::putGlobal(std::string&& key, TjsValue&& value) {
        if(auto tjsVal = getGlobal(key)) {
            delete tjsVal;
            _vm.globals.insert_or_assign(key, new TjsValue{ std::move(value) });
            return;
        }
        _vm.globals.emplace(std::move(key), new TjsValue{ std::move(value) });
    }

    TjsValue* Interpreter::getGlobal(const std::string_view& key) const {
        for(const auto& global : _vm.globals) {
            if(global.first == key) {
                return global.second;
            }
        }
        return nullptr;
    }

    void Interpreter::printStack() {
        fmt::print(
            "+{0:─^{2}}+\n"
            "|{1: ^{2}}|\n"
            "+{0:─^{2}}+\n", "", "stack", 20);
        for(TjsValue* slot = _vm.stack; slot < _vm.sp; slot++) {
            fmt::println("|{0: ^{1}}|", *slot, 20);
        }
        if(_vm.sp == _vm.stack) {
            fmt::println("|{0: ^{1}}|", "empty", 20);
        }
        fmt::println("+{0:─^{1}}+", "", 20);
    }

    void Interpreter::printGlobal() {
        fmt::print(
            "+{0:─^{2}}+\n"
            "|{1: ^{2}}|\n"
            "+{0:─^{2}}+\n", "", "global variable", 40);
        for(auto& [str, val] : _vm.globals) {
            fmt::println("|{0: ^{2}}|{1: ^{2}}|", str, *val, 19);
            fmt::println("+{0:─^{1}}+", "", 40);
        }
    }
}
