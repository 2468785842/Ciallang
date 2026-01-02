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

#include "Function.hpp"
#include "Value.hpp"
#include "vm/VMState.hpp"

namespace Cial {

    class ClassObject final : public Object {
    public:
        ClassMeta *meta;

        explicit ClassObject(ClassMeta *classMeta) : Object(classMeta->className), meta(classMeta) {}

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        void marked() noexcept override {
            Object::marked();
            meta->marked();
        }
    };

    class InstanceObject final : public Object {
    public:
        InstanceObject() = delete;

        explicit InstanceObject(ClassObject *klass) : _class(klass) {}

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        [[nodiscard]] ClassObject *klass() const noexcept { return _class; }

        void marked() noexcept override {
            Object::marked();
            _class->marked();
        }

        void setProp(Atom a, const Value &v);

        Value getProp(Atom a);

        [[nodiscard]] bool hasProp(Atom a) const;

    private:
        ClassObject *_class{ nullptr };
        Map<Atom, Value> _props{};
    };
} // namespace Cial
