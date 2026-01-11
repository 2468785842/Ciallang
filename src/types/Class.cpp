// Copyright (c) 2024/5/21 下午8:44
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

#include "Class.hpp"

#include "Property.hpp"
#include "vm/VMState.hpp"

namespace cial {
    void InstanceObject::setProp(const Atom a, const Value &v) { _props[a] = v; }

    Value InstanceObject::getProp(const Atom a) {
        const auto it = _props.find(a);
        return it != _props.end() ? it->second : Value{};
    }

    bool InstanceObject::hasProp(const Atom a) const { return _class->meta->hasMember(a); }
    // void ClassFunction::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
    //     getFunction()->call(vmState, ret, argCount);
    //     // vmState.curFrame()->thisObj = _thisValue;
    // }

    void ClassObject::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        auto *instanceObj = vmState.rt.create<InstanceObject>(this);

        for(const auto &v : this->meta->memberShapeMetas) {
            if(v.isMethod) {
                auto *funcMeta = this->meta->getMember<FuncMeta>(v.name);
                auto *func = vmState.rt.create<Function>(funcMeta);
                func->thisObj = instanceObj;
                instanceObj->setProp(v.name, Value{ func });
                continue;
            }

            auto *propMeta = this->meta->getMember<PropMeta>(v.name);
            auto *propVal = vmState.rt.create<Property>(instanceObj, propMeta);
            instanceObj->setProp(v.name, Value{ propVal });
        }

        Function constructor{ this->meta->constructor };
        constructor.thisObj = instanceObj;
        constructor.call(vmState, ret, argCount);

        vmState.reg(ret, Value{ instanceObj });
    }

    void InstanceObject::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        throw std::runtime_error("Not implemented InstanceObject call");
    }

} // namespace cial
