/*
 * Copyright (c) 2024/6/8 上午10:23
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */
#pragma once

#include <map>
#include <ranges>

#include "Object.hpp"
#include "Value.hpp"
#include "vm/Constant.hpp"
#include "vm/VMState.hpp"

namespace cial {

    class ClassObject : public Object {
    public:
        ClassMeta *meta;

        explicit ClassObject(ClassMeta *classMeta, Runtime *rt);

        void call(vm::VMState &vmState, u32 ret, size_t argCount) override;

        void marked() noexcept override {
            if(_marked)
                return;
            Object::marked();
            if(meta)
                meta->marked();
            for(auto &v : _props | std::views::values) {
                if(v.isObject()) {
                    v.asObject().value()->marked();
                }
            }
        }

        bool instanceOf(const Atom a) override { return a == Runtime::ATOM_CLASS; }

        static constexpr TypeId nativeTypeId = TypeId::Class;
        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::Class; }

        [[nodiscard]] FuncMeta *getFunc(const Atom a) const {
            const i64 i = this->meta->hasMember(a);
            if(i < 0)
                return nullptr;
            const auto v = this->meta->getMemberShape(i);
            const ClassFieldMeta fieldMeta = this->meta->getMember(i);

            if(v.isMethod) {
                return fieldMeta.funcMeta;
            }
            return nullptr;
        }

        void setProp(Atom a, const Value &v) override;

        Value getProp(Atom a) override;

        [[nodiscard]] bool hasProp(Atom a) const override;

        [[nodiscard]] bool delProp(Atom a) override;

        [[nodiscard]] const Map<Atom, Value> &props() const noexcept { return _props; }
        [[nodiscard]] u64 slotRevision() const noexcept { return _slotRevision; }

    protected:
        Runtime *_rt;
        Map<Atom, Value> _props{}; // static
        u64 _slotRevision{}; // 插入/删除使全局槽缓存失效，更新值不改变槽地址
    };

    class DataObject final : public Object {
    public:
        DataObject() = delete;

        explicit DataObject(vm::VMState *vmState, ClassObject *klass) :
            _vmState(vmState), _class(klass), _finalizeAtom(vmState->rt.wkAtoms.finalize) {}

        // GC 终结走保活队列，在 VM 安全点执行；显式 invalidate 保持同步调用。
        ~DataObject() noexcept override = default;

        [[nodiscard]] bool needsFinalization() const noexcept override;
        [[nodiscard]] bool canFinalizeIn(const vm::VMState &state) const noexcept override {
            return _vmState == &state;
        }
        void finalizeDeferred(vm::VMState &vmState) override;

        void call(vm::VMState &vmState, u32 ret, size_t argCount) override;

        [[nodiscard]] ClassObject *klass() const noexcept { return _class; }

        void marked() noexcept override {
            if(_marked)
                return;
            Object::marked();
            if(_class)
                _class->marked();

            for(auto &v : _props | std::views::values) {
                if(v.isObject()) {
                    v.asObject().value()->marked();
                }
            }

            for(const auto &clazz : _superClass | std::views::values) {
                clazz->marked();
            }
        }

        bool instanceOf(const Atom a) override { return a == _class->meta->className; }

        void setProp(Atom a, const Value &v) override;

        Value getProp(Atom a) override;

        [[nodiscard]] bool hasProp(Atom a) const override;

        [[nodiscard]] bool delProp(Atom a) override;

        void setSuperClass(const Atom a, ClassObject *clazz) {
            if(!_superClass.contains(a))
                _lastSuperClass = clazz;
            _superClass[a] = clazz;
        }

        ClassObject *getSuperClass(const Atom a) {
            if(const auto it = _superClass.find(a); it != _superClass.cend())
                return it->second;
            return nullptr;
        }

        [[nodiscard]] ClassObject *getSuperClass() const { return _lastSuperClass; }

        void invalidate() {
            if(_isValid) {
                // 先复位，finalize 回调期间再次进入也不会重复执行
                _isValid = false;
                const Atom finalizeAtom = _vmState->context.rt().atomTable.intern("finalize"_str);
                // 没有 finalize 成员的类不能碰寄存器池：
                // 旧实现无条件 pushVoid(1) 却只在有 finalize 时 pop(1)，
                // GC sweep 批量析构时池指针会被逐个顶穿
                if(const i64 idx = _class->meta->hasMember(finalizeAtom); idx > -1) {
                    if(!_class->meta->getMemberShape(idx).isMethod ||
                       _class->meta->getMember(idx).funcMeta->syntheticFinalizer)
                        return;
                    const size_t abs = _vmState->getRegPoolTop() - _vmState->curFrame()->getSP();
                    _vmState->pushVoid(1);
                    Function finalizeFun{ _class->meta->getMember(idx).funcMeta };
                    finalizeFun.thisObj = this;
                    finalizeFun.call(*_vmState, static_cast<u32>(abs), 0);
                    _vmState->pop(1);
                }
            }
        }

        [[nodiscard]] bool isValid() const noexcept { return _isValid; }

    private:
        bool _isValid{ true }; // 保留这个字段只是为了兼容性,我们不需要使用它, 主要是配合invalidate
        vm::VMState *_vmState; // 保留vm以便析构函数调用finalize方法, 兼容性
        ClassObject *_class{};
        Atom _finalizeAtom{};
        Map<Atom, Value> _props{};
        Map<Atom, ClassObject *> _superClass{};
        ClassObject *_lastSuperClass{};
    };

    class GlobalObject : public ClassObject {
    public:
        DataObject *proxy{};

        explicit GlobalObject(Runtime *rt) : ClassObject(nullptr, rt) {}

        // Global 判定优先于 Class（继承链上 Global ⊂ Class，dynamic_cast 语义等价）
        static constexpr TypeId nativeTypeId = TypeId::Global;
        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::Global; }

        void marked() noexcept override {
            if(_marked)
                return;
            ClassObject::marked();
            if(proxy)
                proxy->marked();
        }

        Value getProp(const Atom a) override {
            if(const auto it = _props.find(a); it != _props.end())
                return it->second;
            throw std::runtime_error("in global object Not found property");
        }

        // 热路径只读访问:命中时零拷贝返回内部槽引用
        [[nodiscard]] const Value *findPropRef(const Atom a) const noexcept {
            if(const auto it = _props.find(a); it != _props.end())
                return &it->second;
            return nullptr;
        }
    };
} // namespace cial
