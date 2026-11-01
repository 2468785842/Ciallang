// Copyright (c) 2024/5/23 下午9:37
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

#pragma once

#include "../gen/Register.hpp"
#include "Object.hpp"
#include "Value.hpp"
#include "common/Defer.hpp"
#include "vm/Constant.hpp"
#include "vm/VMState.hpp"

namespace cial {

    /**
     * property 触发协议（哪些指令在遇到 Property 对象时调 getter/setter）：
     * - dThis/dGlobal：写 this/global 成员时，槽位为 Property 则 invokeSet
     * - gThis/gGlobal：读 this/global 成员时，槽位为 Property 则 invokeGet
     * - gProp：读对象成员时，槽位为 Property 则 invokeGet
     * - dProp：写对象成员时，槽位为 Property 则 invokeSet
     * - deref_get/deref_set：*p 解引用读写，显式调 getter/setter
     * 纯读写（永不触发）：gLocal/dLocal（局部变量=寄存器拷贝，TJS2 VM_CP 语义）、
     * g_prop_raw/d_prop_raw（& 取本体/绕过 setter 写，旧版 GPDS/SPDS）
     */
    class Property final : public Object {
    public:
        Object *thisObj{};
        PropMeta *propMeta{};

        // 专有 TypeId：loadGlobal/propObjectGet·Set 等热路径用一次虚调用
        // 替代 dynamic_cast 的 RTTI 遍历
        static constexpr TypeId nativeTypeId = TypeId::Property;
        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::Property; }
        bool instanceOf(const Atom name) override { return name == Runtime::ATOM_PROPERTY; }

        explicit Property(Object *thisObj, PropMeta *propMeta) : thisObj{ thisObj }, propMeta{ propMeta } {}

        void invokeSet(vm::VMState &vmState, const Value &v) const {
            if(!propMeta->setFunc)
                throw std::runtime_error("property no have setter");

            const size_t absSP{ vmState.getRegPoolTop() - vmState.curFrame()->getSP() };

            const u32 ret{ static_cast<u32>(absSP) };
            vmState.pushVoid(1);
            vmState.push(v); // absSP + 1

            Function setFunc{ propMeta->setFunc };
            setFunc.thisObj = thisObj;
            setFunc.call(vmState, ret, 1);
        }

        Value invokeGet(vm::VMState &vmState) const {
            if(!propMeta->getFunc)
                throw std::runtime_error("property no have getter");

            // getter 同步重入时，哨兵先截住异常，再由调用方派发循环恢复 catch。
            // 否则嵌套 run 会执行父帧的 catch/后续语句，返回后外层循环再次执行。
            const size_t depth = vmState.context.stackTop();
            auto &boundary = vmState.getterBoundary();
            vmState.allocCallFrame(&boundary);
            vmState.curFrame()->exceptionBoundary = true;
            const size_t base = vmState.curFrame()->getSP();
            DEFER {
                while(vmState.context.stackTop() > depth)
                    vmState.freeCallFrame();
            };
            Function getFunc{ propMeta->getFunc };
            getFunc.thisObj = thisObj;
            getFunc.call(vmState, 0, 0);
            vmState.run();
            if(vmState.context.stackTop() == depth)
                throw vm::ScriptExceptionCarrier{ vmState.poolAt(base + 1) };
            return vmState.reg(0);
        }

        void call(vm::VMState &vmState, u32 ret, size_t argCount) override {
            throw std::runtime_error("Not Support call operator for property");
        }

        void marked() noexcept override {
            if(_marked)
                return;
            MarkSweepHeader::marked(); // 旧实现漏了基类调用，自身永远不会被标记
            if(thisObj)
                thisObj->marked();
            if(propMeta)
                propMeta->marked();
        }
    };

} // namespace cial
