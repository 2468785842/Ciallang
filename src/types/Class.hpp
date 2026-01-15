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

    private:
        Map<Atom, Value> _props{}; // static
    };

    class DataObject final : public Object {
    public:
        DataObject() = delete;

        explicit DataObject(ClassObject *klass) : _class(klass) {}

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

    private:
        ClassObject *_class{ nullptr };
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
    };
} // namespace cial
