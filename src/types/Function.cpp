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

    void Function::call(Bytecode::VMState &vmState, Bytecode::Register ret, const size_t argCount) {
        if(const auto cnt = meta->arity - argCount; cnt > 0)
            vmState.pushVoid(cnt);
        vmState.allocCallFrame(meta->chunk, ret);
        // Faster move Reg window ptr, WARING: reverse args
        auto *currentCallFrame = vmState.curFrame();
        currentCallFrame->baseRegSP -= meta->arity;
    }

    void NativeFunction::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        const auto values = std::make_unique<Value[]>(_arity);
        const Bytecode::CallFrame *curCallFrame = vmState.curFrame();
        const size_t base = vmState.getRegPoolTop() - argCount;
        // Faster operation
        for(std::uint32_t i = 0; i < argCount; i++) {
            new(&values[i]) Value{ curCallFrame->getReg(base - i) };
        }

        const auto &value = callProc(values.get());

        vmState.reg(ret, value);
    }
} // namespace Cial