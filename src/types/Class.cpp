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

#include <unordered_set>

#include "Property.hpp"
#include "vm/VMState.hpp"

namespace cial {

    ClassObject::ClassObject(ClassMeta *classMeta, Runtime *rt) : meta(classMeta), _rt(rt) {}
    namespace {
        void newClass(DataObject *dataObject, const ClassObject *clazz, Bytecode::VMState &vmState, const u32 ret,
                      const size_t argCount) {
            // 1. 定义一个用于追踪已访问类的集合
            std::unordered_set<ClassObject *> visited;

            // 2. 定义递归初始化函数
            auto initRecursive = [&](ClassObject *currentClass, auto &self_ref) -> void {
                if(!currentClass || visited.contains(currentClass)) {
                    return; // 跳过空类或已初始化的类
                }

                // 先递归初始化当前类的所有基类 (Base classes first)
                for(Atom ext : currentClass->meta->extends) {
                    auto *parent = dynamic_cast<ClassObject *>(vmState.global(ext).asObject().unwrap());
                    if(parent) {
                        self_ref(parent, self_ref);
                    }
                }

                for(i64 i = 0; i < currentClass->meta->memberShapeMetas.size(); ++i) {
                    auto memberShapeMeta = currentClass->meta->getMemberShape(i);

                    if(memberShapeMeta.isProp) {
                        const ClassFieldMeta fieldMeta = currentClass->meta->getMember(i);
                        auto *propMeta = fieldMeta.propMeta;
                        auto propVal = vmState.rt.create<Property>(dataObject, propMeta);
                        dataObject->setProp(memberShapeMeta.name, Value{ propVal.get() });
                        continue;
                    }

                    if(memberShapeMeta.isVar) {
                        dataObject->setProp(memberShapeMeta.name, Value{});
                        // continue;
                    }
                }

                // 初始化当前类的默认属性
                assert(currentClass->meta->arity == 0);
                Function init{ currentClass->meta->initDefaultVal };
                init.thisObj = dataObject;
                vmState.allocCallFrame(init.meta);
                auto *currentCallFrame = vmState.curFrame();
                currentCallFrame->thisObj = Value{ init.thisObj };
                vmState.run();
                vmState.freeCallFrame();

                // 标记为已处理
                visited.insert(currentClass);
            };

            // 3. 执行流程
            // 首先，遍历当前类 clazz 的 extends 向量
            for(Atom ext : clazz->meta->extends) {
                auto *baseClass = dynamic_cast<ClassObject *>(vmState.global(ext).asObject().unwrap());
                initRecursive(baseClass, initRecursive);
            }

            // 最后，初始化当前类 (this/clazz) 的属性，确保覆盖基类的同名属性
            {

                for(i64 i = 0; i < clazz->meta->memberShapeMetas.size(); ++i) {
                    auto memberShapeMeta = clazz->meta->getMemberShape(i);

                    if(memberShapeMeta.isProp) {
                        const ClassFieldMeta fieldMeta = clazz->meta->getMember(i);
                        auto *propMeta = fieldMeta.propMeta;
                        auto propVal = vmState.rt.create<Property>(dataObject, propMeta);
                        dataObject->setProp(memberShapeMeta.name, Value{ propVal.get() });
                        continue;
                    }

                    if(memberShapeMeta.isVar) {
                        dataObject->setProp(memberShapeMeta.name, Value{});
                        // continue;
                    }
                }

                assert(clazz->meta->arity == 0);
                Function init{ clazz->meta->initDefaultVal };
                init.thisObj = dataObject;
                vmState.allocCallFrame(init.meta);
                auto *currentCallFrame = vmState.curFrame();
                currentCallFrame->thisObj = Value{ init.thisObj };
                vmState.run();
                vmState.freeCallFrame();
            }

            Function constructor{ clazz->meta->constructor };
            constructor.thisObj = dataObject;

            const auto cnt = clazz->meta->arity > argCount ? clazz->meta->arity - argCount : 0;
            if(cnt > 0)
                vmState.pushVoid(cnt);

            vmState.allocCallFrame(constructor.meta, ret);
            auto *currentCallFrame = vmState.curFrame();
            currentCallFrame->thisObj = Value{ constructor.thisObj };

            vmState.run();

            if(cnt > 0)
                vmState.pop(cnt);
            assert(constructor.meta->chunk->getInstVec().back().opcode() == Inter::OpCode::Ret);
        }
    } // namespace

    /**
     * WARN: This runtime prioritizes behavioral compatibility over architectural elegance.
     */
    void ClassObject::call(Bytecode::VMState &vmState, const u32 ret, const size_t argCount) {

        if(!meta)
            throw std::runtime_error("can't new");

        if(!meta->constructor) {
            throw std::runtime_error("can't new no have constructor");
        }

        // call super class constructor function?
        if(CallFrame *curFrame = vmState.curFrame(); curFrame->thisObj.isObject()) {
            if(auto *dataObject = dynamic_cast<DataObject *>(curFrame->thisObj.asObject().value())) {
                for(const auto &extName : dataObject->klass()->meta->extends) {

                    if(extName == meta->constructor->name) {
                        Function constructor{ meta->constructor };
                        constructor.thisObj = dataObject;

                        const auto cnt = meta->arity > argCount ? meta->arity - argCount : 0;
                        if(cnt > 0)
                            vmState.pushVoid(cnt);

                        vmState.allocCallFrame(constructor.meta, ret);
                        auto *currentCallFrame = vmState.curFrame();
                        currentCallFrame->thisObj = Value{ constructor.thisObj };

                        vmState.run();

                        if(cnt > 0)
                            vmState.pop(cnt);
                        assert(constructor.meta->chunk->getInstVec().back().opcode() == Inter::OpCode::Ret);
                        const Value extVal = vmState.global(extName);
                        assert(extVal.isObject());
                        auto *clazzObject = dynamic_cast<ClassObject *>(extVal.asObject().unwrap());
                        dataObject->setSuperClass(extName, clazzObject);
                        vmState.reg(ret, Value{ dataObject });
                        return;
                    }
                }
            }
        }

        const auto dataObject = vmState.rt.create<DataObject>(&vmState, this);
        newClass(dataObject.get(), this, vmState, ret, argCount);

        for(const Atom extName : meta->extends) {
            if(!dataObject->getSuperClass(extName)) {
                throw std::runtime_error("super class not init in Derived class constructor");
            }
        }

        vmState.reg(ret, Value{ dataObject.get() });
    }

    void ClassObject::setProp(const Atom a, const Value &v) { _props[a] = v; }

    Value ClassObject::getProp(const Atom a) {
        if(const auto it = _props.find(a); it != _props.end())
            return it->second;

        if(meta) {
            if(const i64 i = meta->hasMember(a); i > -1) {
                auto shape = meta->getMemberShape(i);
                if(!shape.isStatic && shape.isMethod) {
                    const auto fun = _rt->create<Function>(meta->getMember(i).funcMeta);
                    return Value{ fun.get() };
                }
            }
        }

        throw std::runtime_error("in class object Not found property");
    }

    bool ClassObject::hasProp(const Atom a) const { return _props.contains(a); }

    void DataObject::setProp(const Atom a, const Value &v) { _props[a] = v; }

    Value DataObject::getProp(const Atom a) {

        if(const auto it = _props.find(a); it != _props.end())
            return it->second;

        if(const i64 i = _class->meta->hasMember(a); i > -1) {
            auto shape = _class->meta->getMemberShape(i);
            if(!shape.isStatic && shape.isMethod) {
                const auto fun = _vmState->context.rt().create<Function>(_class->meta->getMember(i).funcMeta);
                fun->thisObj = this;
                return Value{ fun.get() };
            }
        }

        if(!_superClass.empty() && !_class->meta->extends.empty()) {
            for(const auto &ext : _class->meta->extends | std::views::reverse) {
                if(_vmState->globalHas(ext)) {
                    auto *clazz = dynamic_cast<ClassObject *>(_vmState->global(ext).asObject().unwrap());

                    if(const i64 i = clazz->meta->hasMember(a); i > -1) {
                        auto shape = clazz->meta->getMemberShape(i);
                        if(!shape.isStatic && shape.isMethod) {
                            const auto fun =
                                _vmState->context.rt().create<Function>(clazz->meta->getMember(i).funcMeta);
                            fun->thisObj = this;
                            return Value{ fun.get() };
                        }
                    }
                }
            }
        }

        throw std::runtime_error("in data object Not found property");
    }

    bool DataObject::hasProp(const Atom a) const {

        if(_props.contains(a))
            return true;

        if(const i64 i = _class->meta->hasMember(a); i > -1) {
            auto shape = _class->meta->getMemberShape(i);
            if(!shape.isStatic && shape.isMethod) {
                return true;
            }
        }

        if(!_superClass.empty() && !_class->meta->extends.empty()) {
            for(const auto &ext : _class->meta->extends | std::views::reverse) {
                if(_vmState->globalHas(ext)) {
                    auto *clazz = dynamic_cast<ClassObject *>(_vmState->global(ext).asObject().unwrap());

                    if(const i64 i = clazz->meta->hasMember(a); i > -1) {
                        auto shape = clazz->meta->getMemberShape(i);
                        if(!shape.isStatic && shape.isMethod) {
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    void DataObject::call(Bytecode::VMState &vmState, const u32 ret, const size_t argCount) {
        throw std::runtime_error("Not implemented DataObject call");
    }

} // namespace cial
