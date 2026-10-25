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
#include <fmt/format.h>

#include "Property.hpp"
#include "vm/VMState.hpp"

namespace cial {

    ClassObject::ClassObject(ClassMeta *classMeta, Runtime *rt) : meta(classMeta), _rt(rt) {}
    namespace {
        // 成员字段初始化（newClass 前半段，对构造器无依赖）：基类递归 + 当前类
        // 覆盖 + initDefaultVal 默认值脚本；无构造器类的 new（原版 UB，按实测对齐）单走此段
        void initClassFields(DataObject *dataObject, const ClassObject *clazz, vm::VMState &vmState) {
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
                currentCallFrame->setThisObj(Value{ init.thisObj });
                // run0 执行到帧结束（Ret 或代码耗尽）时自行释放该帧
                vmState.run0();

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
                currentCallFrame->setThisObj(Value{ init.thisObj });
                // run0 执行到帧结束时自行释放该帧
                vmState.run0();
            }
        }

        void newClass(DataObject *dataObject, const ClassObject *clazz, vm::VMState &vmState, const u32 ret,
                      const size_t argCount) {
            initClassFields(dataObject, clazz, vmState);

            Function constructor{ clazz->meta->constructor };
            constructor.thisObj = dataObject;

            // 注意用构造器 FuncMeta 的 arity（ClassMeta::arity 恒 0，是字段初始化器的）：
            // 用错会导致无实参时不补 void，参数寄存器读到上一帧的陈旧值
            const auto cnt = constructor.meta->arity > argCount ? constructor.meta->arity - argCount : 0;
            if(cnt > 0)
                vmState.pushVoid(cnt);

            vmState.allocCallFrame(constructor.meta, ret);
            auto *currentCallFrame = vmState.curFrame();
            currentCallFrame->setThisObj(Value{ constructor.thisObj });

            // 构造函数块保证以 Ret 结尾，run0 执行完自行释放帧
            vmState.run0();

            if(cnt > 0)
                vmState.pop(cnt);
            for(auto &i : constructor.meta->chunk->getInstVec())
            assert(constructor.meta->chunk->getInstVec().back()->opcode() == inter::TacOpCode::Ret);
        }
    } // namespace

    /**
     * WARN: This runtime prioritizes behavioral compatibility over architectural elegance.
     */
    void ClassObject::call(vm::VMState &vmState, const u32 ret, const size_t argCount) {

        if(!meta)
            throw std::runtime_error("The current class cannot be initialized.");

        if(!meta->constructor) {
            // 注意：无构造器类的 new 在原版 TJS2 为未定义行为（无规范语义）。
            // 此处按参考实现实测行为对齐（对拍：仅成员字段初始化（递归含基类），
            // 基类构造器不执行，B.b 保持字段默认值），较旧实现直接报错更宽容，
            // 兼容依赖此形态的脚本；super 解析走 extends 兜底（Super dispatch 已有）
            const auto dataObject = vmState.rt.create<DataObject>(&vmState, this);
            initClassFields(dataObject.get(), this, vmState);
            vmState.regRef(ret) = Value{ dataObject.get() };
            return;
        }

        // call super class constructor function?
        if(CallFrame *curFrame = vmState.curFrame(); curFrame->thisObj.isObject()) {
            if(auto *dataObject = dynamic_cast<DataObject *>(curFrame->thisObj.asObject().value())) {
                for(const auto &extName : dataObject->klass()->meta->extends) {

                    if(extName == meta->constructor->name) {
                        Function constructor{ meta->constructor };
                        constructor.thisObj = dataObject;

                        // 同 newClass：补 void 的数量以构造器 FuncMeta 的 arity 为准
                        const auto cnt = constructor.meta->arity > argCount ? constructor.meta->arity - argCount : 0;
                        if(cnt > 0)
                            vmState.pushVoid(cnt);

                        vmState.allocCallFrame(constructor.meta, ret);
                        auto *currentCallFrame = vmState.curFrame();
                        currentCallFrame->setThisObj(Value{ constructor.thisObj });

                        vmState.run0();

                        if(cnt > 0)
                            vmState.pop(cnt);
                        assert(constructor.meta->chunk->getInstVec().back()->opcode() == inter::TacOpCode::Ret);
                        const Value extVal = vmState.global(extName);
                        assert(extVal.isObject());
                        auto *clazzObject = dynamic_cast<ClassObject *>(extVal.asObject().unwrap());
                        dataObject->setSuperClass(extName, clazzObject);
                        vmState.regRef(ret) = Value{ dataObject };
                        return;
                    }
                }
            }
        }

        const auto dataObject = vmState.rt.create<DataObject>(&vmState, this);
        newClass(dataObject.get(), this, vmState, ret, argCount);

        for(const Atom extName : meta->extends) {
            if(!dataObject->getSuperClass(extName)) {
                throw std::runtime_error("The super class is not initialized in the derived class constructor.");
            }
        }

        vmState.regRef(ret) = Value{ dataObject.get() };
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
            // 构造器不在成员表（Parser 单独分离），但 super.<Ctor>() 形态需要访问：
            // 返回类对象本身（而非 Function），Call 时走 ClassObject::call 的
            // super 构造分支完成 setSuperClass——与全局 A() 直调同路径
            if(a == meta->className && meta->constructor) {
                return Value{ this };
            }
        }

        throw std::runtime_error("in class object Not found property");
    }

    bool ClassObject::hasProp(const Atom a) const { return _props.contains(a); }

    bool ClassObject::delProp(const Atom a) { return _props.erase(a) > 0; }

    void DataObject::setProp(const Atom a, const Value &v) { _props[a] = v; }

    // 仅删除实例自有成员；类定义的方法/属性不删（与 TJS2 delete 语义一致：
    // delete 只作用于对象自身添加的成员）
    bool DataObject::delProp(const Atom a) { return _props.erase(a) > 0; }

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

    void DataObject::call(vm::VMState &vmState, const u32 ret, const size_t argCount) {
        throw std::runtime_error("Not implemented DataObject call");
    }

} // namespace cial
