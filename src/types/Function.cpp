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
        // 超额实参（argCount > arity）：池顶实参区只有末尾 arity 个会落在 %0..，
        // 且 prologue 静态写位（%K-2..）可能进一步覆盖它们——把全部实参按序
        // 复制到窗口顶部保留区（regCount..，CallOmit/CallExpand/CollapseArgs
        // 统一从那里按 call 顺序读取），并把前 arity 个重写回 %0..%arity-1
        // 供 prologue 声明参数搬运
        const size_t excess = argCount > meta->arity ? argCount - meta->arity : 0;
        Vec<Value> rawArgs{};
        if(excess > 0) {
            const Value *raw = vmState.curFrame()->getArgs(argCount);
            rawArgs.assign(raw, raw + argCount);
        }

        // 调用者 this 需在压帧前取（allocCallFrame 后栈顶即新帧）
        const Value callerThis = vmState.curFrame()->thisObj;

        vmState.allocCallFrame(meta, ret, excess > 0 ? argCount : 0);
        auto *currentCallFrame = vmState.curFrame();
        currentCallFrame->actualArgc = static_cast<u32>(argCount);
        if(excess > 0) {
            const u16 tail = meta->chunk->getRegCount();
            for(size_t i = 0; i < meta->arity; ++i)
                vmState.regRef(static_cast<u16>(i)) = rawArgs[i];
            for(size_t j = 0; j < argCount; ++j)
                vmState.regRef(static_cast<u16>(tail + j)) = rawArgs[j];
        }
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