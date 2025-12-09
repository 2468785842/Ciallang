// Copyright (c) 2024/5/23 下午9:37
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

#pragma once

#include "gc/GC.hpp"

namespace Ciallang {
    class Function;
    class Value;

    class Object : public GCObject {
    public:
        Object() = default;
        ~Object() noexcept override = default;

        [[nodiscard]] virtual const char *name() const noexcept = 0;

        [[nodiscard]] virtual size_t arity() const noexcept = 0;
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

        void setMethod(const std::string &name, const Value &fun) noexcept;

        [[nodiscard]] Value getMethod(const std::string &name) const noexcept;

        [[nodiscard]] std::vector<Value> getAllMethods() const noexcept;

    private:
        ClassObject *_base{ nullptr };
        std::string _name;
        size_t _arity{ 0 };
        std::unordered_map<std::string, Value *> _methods{};
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

        [[nodiscard]] const char *name() const noexcept override { return _class ? _class->name() : "instance"; }

        [[nodiscard]] size_t arity() const noexcept override { return 0; }

        [[nodiscard]] ClassObject *klass() const noexcept { return _class; }

    private:
        ClassObject *_class{ nullptr };
    };

} // namespace Ciallang
