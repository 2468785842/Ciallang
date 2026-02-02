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

#include "Class.hpp"
#include "vm/CallFrame.hpp"
#include "vm/Constant.hpp"
#include "vm/VM.hpp"
#include "vm/VMState.hpp"

namespace cial {

    void Function::call(vm::VMState &vmState, const u32 ret, const size_t argCount) {
        const auto cnt = meta->arity > argCount ? meta->arity - argCount : 0;
        if(cnt > 0)
            vmState.pushVoid(cnt);

        vmState.allocCallFrame(meta, ret);
        auto *currentCallFrame = vmState.curFrame();
        currentCallFrame->thisObj = Value{ thisObj };
        // vmState.run();
        //
        // if(cnt > 0)
        //     vmState.pop(cnt);
        // assert(meta->chunk->code().back().opcode == inter::OpCode::Ret);
    }

    void NativeFunction::call(vm::VMState &vmState, const u32 ret, const size_t argCount) {
        const auto cnt = _arity > argCount ? _arity - argCount : 0;
        if(cnt > 0)
            vmState.pushVoid(cnt);

        const auto *curFrame = vmState.curFrame();
        VM vm{ &vmState };

        const auto &value = callProc(&vm, argCount, curFrame->getArgs(cnt != 0 ? _arity : argCount));

        vmState.regRef(ret) = value;

        if(cnt > 0)
            vmState.pop(cnt);
    }
} // namespace cial