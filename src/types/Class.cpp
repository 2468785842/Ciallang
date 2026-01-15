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
    namespace {
        void initField(Runtime &rt, Object *thisObj, const MemberShapeMeta &memberShapeMeta,
                       const ClassFieldMeta &fieldMeta) {
            if(memberShapeMeta.isMethod) {
                auto *funcMeta = fieldMeta.funcMeta;
                auto *func = rt.create<Function>(funcMeta);
                func->thisObj = thisObj;
                thisObj->setProp(memberShapeMeta.name, Value{ func });
                return;
            }

            if(memberShapeMeta.isProp) {
                auto *propMeta = fieldMeta.propMeta;
                auto *propVal = rt.create<Property>(thisObj, propMeta);
                propVal->thisObj = thisObj;
                thisObj->setProp(memberShapeMeta.name, Value{ propVal });
                return;
            }

            if(memberShapeMeta.isVar) {
                thisObj->setProp(memberShapeMeta.name, Value{});
            }
        }
    } // namespace

    ClassObject::ClassObject(ClassMeta *classMeta, Runtime &rt) : meta(classMeta) {
        for(auto &v : classMeta->memberShapeMetas) {
            if(!v.isStatic)
                continue;
            const ClassFieldMeta fieldMeta = this->meta->getMember(v.name);
            initField(rt, this, v, fieldMeta);
        }
    }

    void ClassObject::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        if(!meta)
            throw std::runtime_error("can't new");

        auto *dataObject = vmState.rt.create<DataObject>(this);

        for(const auto &v : this->meta->memberShapeMetas) {
            if(v.isStatic)
                continue;
            const ClassFieldMeta fieldMeta = this->meta->getMember(v.name);
            initField(vmState.rt, dataObject, v, fieldMeta);
        }

        Function constructor{ this->meta->constructor };
        constructor.thisObj = dataObject;
        constructor.call(vmState, ret, argCount);

        vmState.reg(ret, Value{ dataObject });
    }

    void ClassObject::setProp(const Atom a, const Value &v) { _props[a] = v; }

    Value ClassObject::getProp(const Atom a) {
        if(const auto it = _props.find(a); it != _props.end())
            return it->second;
        throw std::runtime_error("Not found property");
    }

    bool ClassObject::hasProp(const Atom a) const { return _props.contains(a); }

    void DataObject::setProp(const Atom a, const Value &v) { _props[a] = v; }

    Value DataObject::getProp(const Atom a) {
        if(const auto it = _props.find(a); it != _props.end())
            return it->second;
        throw std::runtime_error("Not found property");
    }

    bool DataObject::hasProp(const Atom a) const { return _props.contains(a); }

    void DataObject::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        throw std::runtime_error("Not implemented DataObject call");
    }

} // namespace cial
