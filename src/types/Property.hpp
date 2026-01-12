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

#include "Object.hpp"
#include "Value.hpp"
#include "vm/Constant.hpp"
#include "vm/Register.hpp"
#include "vm/VMState.hpp"

namespace cial {

    /**
     * mov r1, r2
     * dThis r1, r2
     * dGlobal r1, r2
     * When this instruction is executed, it checks whether r2 is a Property object.
     * If so, invokeSet is called on r2, with r1 passed as the argument.
     *
     * cp r1, r2
     * gThis r1, r2
     * gGlobal r1, r2
     * When this instruction is executed, it checks whether r1 is a Property object.
     * If so, invokeGet is called on r1, and the returned value is stored in r2.
     *
     * gProp r1, r2, r3
     * When this instruction is executed, it checks whether r2 is a Property object.
     * If so, invokeGet is called on r2, and the returned value is stored in r3.
     *
     * dProp r1, r2, r3
     * When this instruction is executed, it checks whether r2 is a Property object.
     * If so, invokeSet is called on r2, with r3 passed as the argument.
     */
    class Property final : public Object {
    public:
        Object *thisObj{};
        PropMeta *propMeta{};

        explicit Property(Object *thisObj, PropMeta *propMeta) : thisObj{ thisObj }, propMeta{ propMeta } {}

        void invokeSet(Bytecode::VMState &vmState, const Value &v) const {
            if(!propMeta->setFunc)
                throw std::runtime_error("property no have setter");

            const size_t absSP{ vmState.getRegPoolTop() - vmState.curFrame()->getSP() };

            const Bytecode::Register ret{ absSP };
            vmState.pushVoid(1);
            vmState.push(v); // absSP + 1

            Function setFunc{ propMeta->setFunc };
            setFunc.thisObj = thisObj;
            setFunc.call(vmState, ret, 1);
        }

        Value invokeGet(Bytecode::VMState &vmState) const {
            if(!propMeta->getFunc)
                throw std::runtime_error("property no have getter");

            const size_t absSP{ vmState.getRegPoolTop() - vmState.curFrame()->getSP() };

            const Bytecode::Register ret{ absSP };
            vmState.pushVoid(1);

            Function getFunc{ propMeta->getFunc };
            getFunc.thisObj = thisObj;
            getFunc.call(vmState, ret, 0);
            return vmState.reg(ret);
        }

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override {
            throw std::runtime_error("Not Support call operator for property");
        }

        void marked() noexcept override {
            if(thisObj)
                thisObj->marked();
            if(propMeta)
                propMeta->marked();
        }
    };

} // namespace cial
