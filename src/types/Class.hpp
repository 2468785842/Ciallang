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

    class ClassFunction final : public Object {
    public:
        ClassFunction(const Value &thisValue, const Value &function) : _thisValue(thisValue), _function(function) {}

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        [[nodiscard]] Function *getFunction() const noexcept { return dynamic_cast<Function *>(_function.toObject()); }

        // Opt<Vec<GCObject *>> getRefs() override {
        //     auto refs = _function.toObject()->getRefs();
        //     refs->push_back(toGCObject(_thisValue));
        //     return refs;
        // }

    private:
        Value _thisValue;
        Value _function;
    };

    struct FieldMeta {
        std::optional<Bytecode::Register> defValReg{};
    };

    class ClassObject final : public Object {
    public:
        ClassObject() = delete;

        explicit ClassObject(Atom name, const size_t arity = 0) : Object(name), _arity(arity) {}

        ~ClassObject() noexcept override;

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        void setBase(ClassObject *base) noexcept;

        [[nodiscard]] ClassObject *base() const noexcept { return _base; }

        void setStaticField(Atom name, const Value &field) noexcept;

        [[nodiscard]] Value getStaticField(Atom name) const noexcept;

        void setMethod(Atom name, const Value &method) noexcept;

        [[nodiscard]] Value getMethod(Atom name) const noexcept;

        [[nodiscard]] std::vector<Value> getMethods() const noexcept;

        void setFieldDef(Atom name, const FieldMeta &fieldMeta) noexcept;

        [[nodiscard]] FieldMeta getFieldDef(Atom name) const noexcept;

        [[nodiscard]] const std::unordered_map<Atom, FieldMeta> &getFieldDefs() const noexcept { return _fieldsDef; }

        // Opt<Vec<GCObject *>> getRefs() override {
        //     Vec<GCObject *> refs{};
        //
        //     if(_base) {
        //         if(const auto baseOptRefs = _base->getRefs()) {
        //             refs = *baseOptRefs;
        //         }
        //     }
        //
        //     for(auto &v : std::views::values(_methodsDef)) {
        //         refs.push_back(toGCObject(v));
        //     }
        //
        //     for(auto &v : std::views::values(_staticFields)) {
        //         refs.push_back(toGCObject(v));
        //     }
        //
        //     return refs;
        // }

    private:
        ClassObject *_base{ nullptr };
        size_t _arity{ 0 };
        std::unordered_map<Atom, Value> _methodsDef{};
        std::unordered_map<Atom, FieldMeta> _fieldsDef{};
        std::unordered_map<Atom, Value> _staticFields{};
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
