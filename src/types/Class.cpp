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

        // call super class constructor function?
        if(vmState.curFrame()->thisObj.isObject()) {
            if(auto *dataObject = dynamic_cast<DataObject *>(vmState.curFrame()->thisObj.asObject().value())) {
                for(const auto &extName : dataObject->klass()->meta->extends) {
                    auto *clazz = this;
                    if(!clazz)
                        break;

                    if(extName == clazz->meta->constructor->name) {
                        clazz->call(vmState, ret, argCount);
                        auto *superDataObject = dynamic_cast<DataObject *>(vmState.reg(ret).asObject().unwrap());
                        superDataObject->fallbackDataObject = dataObject;
                        dataObject->setSuperDataClass(extName, superDataObject);
                        return;
                    }
                }
            }
        }

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

        for(Atom extName : this->meta->extends) {
            if(dataObject->superClass().contains(extName))
                continue;
            // try auto call super class constructor

            // call super class constructor function?
            auto *clazz = dynamic_cast<ClassObject *>(vmState.global(extName).asObject().unwrap());
            if(!clazz)
                throw std::runtime_error("Not found class");
            if(clazz->meta->constructor->arity == 0) {
                if(extName == clazz->meta->constructor->name) {
                    clazz->call(vmState, ret, argCount);
                    auto *superDataObject = dynamic_cast<DataObject *>(vmState.reg(ret).asObject().unwrap());
                    superDataObject->fallbackDataObject = dataObject;
                    dataObject->setSuperDataClass(extName, superDataObject);
                }
            } else {
                throw std::runtime_error("constructor args count not zero can't auto call");
            }
        }

        vmState.reg(ret, Value{ dataObject });
    }

    void ClassObject::setProp(const Atom a, const Value &v) { _props[a] = v; }

    Value ClassObject::getProp(const Atom a) {
        if(const auto it = _props.find(a); it != _props.end())
            return it->second;
        throw std::runtime_error("in class object Not found property");
    }

    bool ClassObject::hasProp(const Atom a) const { return _props.contains(a); }

    void DataObject::setProp(const Atom a, const Value &v) {
        if(const auto it = _props.find(a); it != _props.end()) {
            _props[a] = v;
            return;
        }

        // when _superClass is empty
        // it may indicate that the object is in a newly created state.
        if(!_superClass.empty() && !_class->meta->extends.empty()) {
            for(const auto &ext : _class->meta->extends | std::views::reverse) {
                if(_superClass[ext]->hasProp(a)) {
                    _superClass[ext]->setProp(a, v);
                    return;
                }
            }
        }

        if(fallbackDataObject) {
            if(const auto it = fallbackDataObject->_props.find(a); it != fallbackDataObject->_props.end()) {
                fallbackDataObject->_props[a] = v;
                return;
            }
        }

        _props[a] = v;
    }

    Value DataObject::getProp(const Atom a) {
        if(const auto it = _props.find(a); it != _props.end())
            return it->second;

        if(!_class->meta->extends.empty()) {
            for(const auto &ext : _class->meta->extends | std::views::reverse) {
                if(_superClass.at(ext)->hasProp(a)) {
                    return _superClass.at(ext)->getProp(a);
                }
            }
        }

        if(fallbackDataObject) {
            if(const auto it = fallbackDataObject->_props.find(a); it != fallbackDataObject->_props.end())
                return it->second;
        }

        throw std::runtime_error("in data object Not found property");
    }

    bool DataObject::hasProp(const Atom a) const {
        if(_props.contains(a))
            return true;

        if(!_class->meta->extends.empty()) {
            for(const auto &ext : _class->meta->extends | std::views::reverse) {
                if(_superClass.at(ext)->hasProp(a)) {
                    return true;
                }
            }
        }

        if(fallbackDataObject) {
            if(fallbackDataObject->_props.contains(a))
                return true;
        }

        return false;
    }

    void DataObject::call(Bytecode::VMState &vmState, const Bytecode::Register ret, const size_t argCount) {
        throw std::runtime_error("Not implemented DataObject call");
    }

} // namespace cial
