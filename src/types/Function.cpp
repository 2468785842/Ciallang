// Copyright (c) 2025/9/29 16:04
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

//
// Created by LiDon on 2025/9/29.
//

#include "Function.hpp"

#include "vm/VMState.hpp"

namespace Cial {

    Function::Function(Bytecode::Chunk *chunk, const std::string &name) : Function(chunk, name, 0) {}

    Function::Function(Bytecode::Chunk *chunk, std::string name, const size_t arity) :
        _chunk(chunk), _name(std::move(name)), _arity(arity) {}

    void Function::call(Bytecode::VMState &vmState, Bytecode::Register ret, const size_t argCount) {
        const auto cnt = static_cast<std::int64_t>(_arity - argCount);
        if(cnt > 0)
            vmState.pushVoid(cnt);
        vmState.allocCallFrame(_chunk.get(), ret);
        // Faster move Reg window ptr, WARING: reverse args
        auto *currentCallFrame = vmState.current();
        currentCallFrame->baseRegSP -= _arity;
    }

    void NativeFunction::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        const auto values = std::make_unique<Value[]>(_arity);
        const Bytecode::CallFrame *curCallFrame = vmState.current();
        const size_t base = vmState.getRegPoolTop() - argCount;
        // Faster operation
        for(std::uint32_t i = 0; i < argCount; i++) {
            new(&values[i]) Value{ curCallFrame->getReg(base - i) };
        }

        const auto &value = callProc(values.get());

        vmState.reg(ret, value);
    }
} // namespace Cial