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
    Interpreter::Interpreter(Common::SourceFile& sourceFile, VMChunk&& chunk)
        : _main(new TjsFunction{ std::move(chunk), "main" }),
          _sourceFile(sourceFile) {
        _vm.frames[0] = CallFrame{
                .vmChunk = _main->chunk().get(),
                .ip = 0,
                .slots = _vm.stack
        };
    }

    InterpretResult Interpreter::run(Common::Result& r) {
        for(;;) {
            auto opcode = static_cast<Opcode>(chunk()->bytecodes(callFrame().ip));

            const auto* ins = Instruction::instance(opcode);

            LOG(INFO) << ins->disassemble(this, false);

            ip(1);

            auto result = ins->execute(r, this);
            if(result != InterpretResult::CONTINUE) {
                LOG(INFO) << "VM return";

                return result;
            }
        }
    }

    uint8_t Interpreter::readByte() {
        return chunk()->bytecodes(_vm.frames[_vm.frameCount].ip++);
    }

    const TjsValue& Interpreter::readConstant(const size_t index) const {
        return chunk()->constants(index);
    }

    const TjsValue& Interpreter::peek(const size_t distance) const {
        DCHECK_GE(
            _vm.sp - _vm.stack - static_cast<intptr_t>(distance) - 1, 0
        ) << "stack underflow `peek(distance)` method";

        DCHECK_LE(
            _vm.sp - _vm.stack - static_cast<intptr_t>(distance) - 1, STACK_MAX - 1
        ) << "stack overflow `peek(distance)` method";

        return _vm.sp[distance - 1];
    }

    void Interpreter::pushVoid() {
        DCHECK_LE(_vm.sp - _vm.stack + 1, STACK_MAX - 1)
            << "stack overflow `push(value)` method";
        *_vm.sp++ = TjsValue{};
    }

    void Interpreter::push(TjsValue&& value) {
        DCHECK_LE(
            _vm.sp - _vm.stack + 1, STACK_MAX - 1
        ) << "stack overflow `push(value)` method";
        *_vm.sp++ = std::move(value);
    }

    TjsValue Interpreter::popN(const size_t n) {
        DCHECK_GE(
            reinterpret_cast<std::uintptr_t>(_vm.sp - n),
            reinterpret_cast<std::uintptr_t>(_vm.stack)
        ) << "stack underflow! `popN` method";
        _vm.sp -= n;
        return std::move(*_vm.sp);
    }

    TjsValue Interpreter::pop() {
        DCHECK_GE(
            reinterpret_cast<std::uintptr_t>(_vm.sp - 1),
            reinterpret_cast<std::uintptr_t>(_vm.stack)
        ) << "stack underflow! `pop` method";
        return std::move(*--_vm.sp);
    }

    TjsValue* Interpreter::popArgs(const size_t count) {
        DCHECK_GE(
            reinterpret_cast<std::uintptr_t>(_vm.sp - count),
            reinterpret_cast<std::uintptr_t>(_vm.stack)
        ) << "stack underflow! `popN` method";
        _vm.sp -= count;
        return _vm.sp;
    }

    const TjsValue& Interpreter::getStack(const size_t slot) const {
        if(_vm.frames[_vm.frameCount].slots == _vm.sp) {
            return TjsValue{}; // dying value
        }
        return _vm.frames[_vm.frameCount].slots[slot];
    }

    void Interpreter::putStack(const size_t slot, TjsValue&& value) const {
        _vm.frames[_vm.frameCount].slots[slot] = std::move(value);
    }

    void Interpreter::addNative(const TjsNativeFunction& nFun) {
        _vm.globals.emplace(nFun.name(), new TjsValue{ nFun });
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
            "+{0:─^{2}}+\n", "", "global variable", 41);
        for(auto& [str, val] : _vm.globals) {
            fmt::println("|{0: ^{2}}|{1: ^{2}}|", str, *val, 20);
            fmt::println("+{0:─^{1}}+", "", 41);
        }
    }

    void Interpreter::dump() {
        const auto* rlc = chunk()->rlc();

        #define RLC_NAME_LEN (rlc->name().length() < 35 ? 35 - rlc->name().length() : 35)
        #define RLC_NAME (RLC_NAME_LEN == 35 ? "-" : rlc->name())

        fmt::println("{0:-^{1}}", ' ' + RLC_NAME + ' ', RLC_NAME_LEN);

        while(ip() < chunk()->count()) {
            auto op = static_cast<Opcode>(chunk()->bytecodes(callFrame().ip));
            const auto* ins = Instruction::instance(op);
            fmt::println("{}", ins->disassemble(this));
            intptr_t index = ins->length();
            if(index == -1) {
                index = disassembleExtIndex<OpcodeMode::IEX>(*chunk(), ip());
            }
            ip(index);
        }
    }
}
