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

#include "Function.hpp"
#include "Value.hpp"
#include "vm/VMState.hpp"

namespace Cial {

    class Property final : public Object {
    public:
        Property(const Value &thisValue, const Value &function) : _thisValue(thisValue), _function(function) {}

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        [[nodiscard]] Function *getFunction() const noexcept { return dynamic_cast<Function *>(_function.toObject()); }

    private:
        Value _thisValue;
        Value _function;
    };

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

        explicit InstanceObject(ClassObject *klass) : _class(klass) {
            if(_class) {
                // _class->incRef();
            }
        }

        ~InstanceObject() noexcept override {
            if(_class) {
                // _class->decRef();
            }
        }

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        [[nodiscard]] ClassObject *klass() const noexcept { return _class; }

        void setField(Atom name, const Value &field) noexcept;

        [[nodiscard]] Value getField(Atom name) const noexcept;

        void setMethod(Atom name, const Value &method) noexcept;

        [[nodiscard]] Value getMethod(Atom name) const noexcept;

        // Opt<Vec<GCObject *>> getRefs() override {
        //     Vec<GCObject *> refs{};
        //
        //     if(_class) {
        //         if(const auto classOptRefs = _class->getRefs()) {
        //             refs = *classOptRefs;
        //         }
        //     }
        //
        //     for(auto &v : std::views::values(_fields)) {
        //         refs.push_back(toGCObject(v));
        //     }
        //
        //     for(auto &v : std::views::values(_fields)) {
        //         refs.push_back(toGCObject(v));
        //     }
        //
        //     return refs;
        // }

    private:
        ClassObject *_class{ nullptr };
        std::unordered_map<Atom, Value> _fields{};
        std::unordered_map<Atom, Value> _methods{};
    };
} // namespace Cial
