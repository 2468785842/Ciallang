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

        // 调用者 this 需在压帧前取（allocCallFrame 后栈顶即新帧）
        const Value callerThis = vmState.curFrame()->thisObj;

        vmState.allocCallFrame(meta, ret);
        auto *currentCallFrame = vmState.curFrame();
        if(thisObj) {
            currentCallFrame->setThisObj(Value{ thisObj });
        } else if(callerThis.isObject()) {
            // TJS2 调用点绑定语义：clo.ObjThis 为空时 this 取调用者的 this
            //（super.<方法>() 经类对象成员访问得到未绑定函数，需沿调用链继承；
            // 顶层/全局函数的 this 本就是全局对象，行为不变）
            currentCallFrame->setThisObj(callerThis);
        } else {
            // 全局函数调用(绝大多数):免 Value 装配与 dynamic_cast
            currentCallFrame->setThisObjNull();
        }
        // vmState.run();
        //
        // if(cnt > 0)
        //     vmState.pop(cnt);
        // assert(meta->chunk->getInstVec().back().opcode == inter::OpCode::Ret);
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