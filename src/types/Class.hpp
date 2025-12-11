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

namespace Ciallang {

    struct FieldMeta {
        std::optional<Bytecode::Register> defValReg{};
    };

    class ClassObject final : public Object {
    public:
        ClassObject() = delete;

        explicit ClassObject(std::string name, const size_t arity = 0) : _name(std::move(name)), _arity(arity) {}

        ~ClassObject() noexcept override;

        [[nodiscard]] const char *name() const noexcept override { return _name.c_str(); }

        [[nodiscard]] size_t arity() const noexcept override { return _arity; }

        void setBase(ClassObject *base) noexcept;

        [[nodiscard]] ClassObject *base() const noexcept { return _base; }

        void setMethod(const std::string &name, const Value &method) noexcept;

        [[nodiscard]] Value getMethod(const std::string &name) const noexcept;

        [[nodiscard]] std::vector<Value> getAllMethod() const noexcept;

        void setFieldDef(const std::string &name, const FieldMeta &fieldMeta) noexcept;

        [[nodiscard]] FieldMeta getFieldDef(const std::string &name) const noexcept;

        [[nodiscard]] const std::unordered_map<std::string, FieldMeta> &getFieldDefs() const noexcept {
            return _fieldDefs;
        }

    private:
        ClassObject *_base{ nullptr };
        std::string _name;
        size_t _arity{ 0 };
        std::unordered_map<std::string, Value> _methods{};
        std::unordered_map<std::string, Value> _staticMethods{};
        std::unordered_map<std::string, FieldMeta> _fieldDefs{};
        std::unordered_map<std::string, Value> _staticFields{};
    };

    class InstanceObject final : public Object {
    public:
        InstanceObject() = delete;

        explicit InstanceObject(ClassObject *klass) : _class(klass) {
            if(_class) {
                _class->incRef();
            }
        }

        ~InstanceObject() noexcept override {
            if(_class) {
                _class->decRef();
            }
        }

        [[nodiscard]] const char *name() const noexcept override { return _class ? _class->name() : "unknown"; }

        [[nodiscard]] size_t arity() const noexcept override { return 0; }

        [[nodiscard]] ClassObject *klass() const noexcept { return _class; }

        void setField(const std::string &name, const Value &field) noexcept;

        [[nodiscard]] Value getField(const std::string &name) const noexcept;

    private:
        ClassObject *_class{ nullptr };
        std::unordered_map<std::string, Value> _fields{};
    };
} // namespace Ciallang
