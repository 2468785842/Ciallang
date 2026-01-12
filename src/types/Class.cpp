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

    void ClassObject::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        auto *instanceObj = vmState.rt.create<InstanceObject>(this);

        for(const auto &v : this->meta->memberShapeMetas) {
            const ClassFieldMeta fieldMeta = this->meta->getMember(v.name);
            if(v.isMethod) {
                auto *funcMeta = fieldMeta.funcMeta;
                auto *func = vmState.rt.create<Function>(funcMeta);
                func->thisObj = instanceObj;
                instanceObj->setProp(v.name, Value{ func });
                continue;
            }

            if(v.isProp) {
                auto *propMeta = fieldMeta.propMeta;
                auto *propVal = vmState.rt.create<Property>(instanceObj, propMeta);
                propVal->thisObj = instanceObj;
                instanceObj->setProp(v.name, Value{ propVal });
                continue;
            }

            if(v.isVar) {
                instanceObj->setProp(v.name, Value{});
            }
        }

        Function constructor{ this->meta->constructor };
        constructor.thisObj = instanceObj;
        constructor.call(vmState, ret, argCount);

        vmState.reg(ret, Value{ instanceObj });
    }

    void InstanceObject::setProp(const Atom a, const Value &v) { _props[a] = v; }

    Value InstanceObject::getProp(const Atom a) {
        const auto it = _props.find(a);
        return it != _props.end() ? it->second : Value{};
    }

    bool InstanceObject::hasProp(const Atom a) const { return _class->meta->hasMember(a); }

    void InstanceObject::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        throw std::runtime_error("Not implemented InstanceObject call");
    }

} // namespace cial
