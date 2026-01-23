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

        bool instanceOf(const Atom a) override { return a == Runtime::ATOM_CLASS; }

        [[nodiscard]] FuncMeta *getFunc(const Atom a) const {
            const std::int64_t i = this->meta->hasMember(a);
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

        [[nodiscard]] const Map<Atom, Value> &props() const noexcept { return _props; }

    protected:
        Runtime *_rt;
        Map<Atom, Value> _props{}; // static
    };

    class DataObject final : public Object {
    public:
        DataObject() = delete;

        explicit DataObject(Bytecode::VMState *vmState, ClassObject *klass) : _vmState(vmState), _class(klass) {}

        ~DataObject() noexcept override { invalidate(); }

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        [[nodiscard]] ClassObject *klass() const noexcept { return _class; }

        void marked() noexcept override {
            Object::marked();
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
                static const Atom finalizeAtom = _vmState->context.rt().atomTable.intern("finalize"_str);
                const size_t abs = _vmState->getRegPoolTop() - _vmState->curFrame()->getSP();
                _vmState->pushVoid(1);
                // if not found finalize, may ignore??
                if(const std::int64_t idx = _class->meta->hasMember(finalizeAtom); idx > -1) {
                    Function finalizeFun{ _class->meta->getMember(idx).funcMeta };
                    finalizeFun.thisObj = this;
                    finalizeFun.call(*_vmState, Bytecode::Register{ abs }, 0);
                    _vmState->pop(1);
                }
            }
            _isValid = false;
        }

        [[nodiscard]] bool isValid() const noexcept { return _isValid; }

    private:
        bool _isValid{ true }; // 保留这个字段只是为了兼容性,我们不需要使用它, 主要是配合invalidate
        Bytecode::VMState *_vmState; // 保留vm以便析构函数调用finalize方法, 兼容性
        ClassObject *_class{};
        Map<Atom, Value> _props{};
        Map<Atom, ClassObject *> _superClass{};
        ClassObject *_lastSuperClass{};
    };

    class GlobalObject : public ClassObject {
    public:
        DataObject *proxy{};

        explicit GlobalObject(Runtime *rt) : ClassObject(nullptr, rt) {}

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
