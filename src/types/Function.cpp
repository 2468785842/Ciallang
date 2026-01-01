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

#include "Object.hpp"

#include "vm/CallFrame.hpp"
#include "vm/Constant.hpp"
#include "vm/VMState.hpp"

namespace Cial {

    void Function::call(Bytecode::VMState &vmState, Bytecode::Register ret, const size_t argCount) {
        const auto cnt = meta->arity - argCount;
        if(cnt > 0)
            vmState.pushVoid(cnt);

        vmState.allocCallFrame(meta, ret);
        // Fast move Reg window ptr, WARING: reverse args
        auto *currentCallFrame = vmState.curFrame();
        currentCallFrame->context = getContext();
        vmState.run();

        if(cnt > 0)
            vmState.pop(cnt);
    }

    void NativeFunction::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {

        const auto cnt = _arity - argCount;
        if(cnt > 0)
            vmState.pushVoid(cnt);

        const size_t used = vmState.getRegPoolTop();

        const auto &value = callProc(&vmState.curFrame()->getReg(Bytecode::Register{ used - argCount }));

        vmState.reg(ret, value);

        if(cnt > 0)
            vmState.pop(cnt);
    }
} // namespace Cial