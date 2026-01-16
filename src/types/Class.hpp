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

#include <ranges>

#include "Object.hpp"
#include "Value.hpp"
#include "vm/Constant.hpp"
#include "vm/VMState.hpp"

namespace cial {

    class ClassObject : public Object {
    protected:
        explicit ClassObject() : meta(nullptr) {}

    public:
        ClassMeta *meta;

        explicit ClassObject(ClassMeta *classMeta, Runtime &rt);

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        void marked() noexcept override {
            Object::marked();
            if(meta)
                meta->marked();
            for(auto &v : _props | std::views::values) {
                if(v.isObject()) {
                    v.asObject().value()->marked();
                }
            }
        }

        void setProp(Atom a, const Value &v) override;

        Value getProp(Atom a) override;

        [[nodiscard]] bool hasProp(Atom a) const override;

        [[nodiscard]] const Map<Atom, Value> &props() const noexcept { return _props; }

    protected:
        Map<Atom, Value> _props{}; // static
    };

    /**
     * NOTE: First, it's worth mentioning that inheritance in TJS2 is not true inheritance
     * NOTE: but rather more like composition;
     * NOTE: each super class instance has its own fields.
     */
    class DataObject final : public Object {
    public:
        DataObject *fallbackDataObject{}; // 当前实例是从那个对象构造的?指向子类实例
        DataObject() = delete;

        explicit DataObject(Bytecode::VMState *vmState, ClassObject *klass) : _vmState(vmState), _class(klass) {}

        // ~DataObject() noexcept override { invalidate(); }

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        [[nodiscard]] ClassObject *klass() const noexcept { return _class; }

        void marked() noexcept override {
            Object::marked();
            _class->marked();
            if(fallbackDataObject)
                fallbackDataObject->marked();

            for(auto &v : _props | std::views::values) {
                if(v.isObject()) {
                    v.asObject().value()->marked();
                }
            }

            for(const auto &v : _superClass | std::views::values) {
                v->marked();
            }
        }

        void setProp(Atom a, const Value &v) override;

        Value getProp(Atom a) override;

        [[nodiscard]] bool hasProp(Atom a) const override;

        void setSuperDataClass(const Atom a, DataObject *superClass) { _superClass[a] = superClass; }

        DataObject *getSuperDataClass(const Atom a) { return _superClass[a]; }

        [[nodiscard]] const Map<Atom, DataObject *> &superClass() const noexcept { return _superClass; }

        void invalidate() {
            if(!_isValid) {
                static const auto finalizeAtom = _vmState->context.rt().atomTable.intern("finalize"_str);
                if(_props.contains(finalizeAtom)) {
                    const size_t abs = _vmState->getRegPoolTop() - _vmState->curFrame()->getSP();
                    _vmState->pushVoid(1);
                    _props[finalizeAtom].asObject().unwrap()->call(*_vmState, Bytecode::Register{ abs }, 0);
                    _vmState->pop(1);
                }
            }
            _isValid = true;
        }

        bool isValid() const noexcept { return _isValid; }

    private:
        bool _isValid{ false }; // 保留这个字段只是为了兼容性,我们不需要使用它, 主要是配合invalidate
        Bytecode::VMState *_vmState; // 保留vm以便析构函数调用finalize方法, 兼容性
        ClassObject *_class{};
        Map<Atom, Value> _props{};
        Map<Atom, DataObject *> _superClass{};
    };

    class GlobalObject : public ClassObject {
    public:
        DataObject *proxy{};

        explicit GlobalObject() = default;

        void marked() noexcept override {
            ClassObject::marked();
            if(proxy)
                proxy->marked();
        }

        Value getProp(const Atom a) override {
            if(const auto it = _props.find(a); it != _props.end())
                return it->second;
            throw std::runtime_error("in global object Not found property");
        }
    };
} // namespace cial
