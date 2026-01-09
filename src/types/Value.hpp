// Copyright (c) 2024/5/23 下午9:09
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

#include <fmt/ostream.h>

#include "Types.hpp"

#include "Real.hpp"

#include "common/Ret.hpp"

namespace cial {
    class Value {

    public:
        Value() = default;

        explicit Value(Integer value);

        explicit Value(Real value);

        explicit Value(String *value);

        explicit Value(Octet *value);

        explicit Value(Object *value);

        ~Value();

        Value(const Value &v) noexcept;

        Value &operator=(const Value &value) noexcept;

        [[nodiscard]] ValueType type() const noexcept { return _type; }

        void type(const ValueType type) noexcept { _type = type; }

        [[nodiscard]] bool isVoid() const noexcept { return type() == ValueType::Void; }

        [[nodiscard]] bool isInteger() const noexcept { return type() == ValueType::Integer; }

        [[nodiscard]] bool isReal() const noexcept { return type() == ValueType::Real; }

        [[nodiscard]] bool isString() const noexcept { return type() == ValueType::String; }

        [[nodiscard]] bool isOctet() const noexcept { return type() == ValueType::Octet; }

        [[nodiscard]] bool isObject() const noexcept { return type() == ValueType::Object; }

        [[nodiscard]] const char *name() const;

        [[nodiscard]] Ret<Integer> asInteger() const noexcept;

        [[nodiscard]] Ret<Real> asReal() const noexcept;

        [[nodiscard]] Ret<String *> asString() const noexcept;

        [[nodiscard]] Ret<Octet *> asOctet() const noexcept;

        [[nodiscard]] Ret<Object *> asObject() const noexcept;

        [[nodiscard]] bool asBool() const noexcept;

        [[nodiscard]] Ret<void> toInteger() noexcept;

        [[nodiscard]] Ret<void> toReal() noexcept;

        [[nodiscard]] Ret<void> toString() noexcept;

        [[nodiscard]] Ret<void> toOctet() const noexcept;

        [[nodiscard]] Ret<void> toObject() const noexcept;

        void toLogicalNot() noexcept;

        [[nodiscard]] Ret<void> toSignChange() noexcept;

        [[nodiscard]] Ret<Value> add(const Value &value) const noexcept;

        [[nodiscard]] Ret<Value> sub(const Value &value) const noexcept;
        [[nodiscard]] Ret<Value> mul(const Value &value) const noexcept;
        [[nodiscard]] Ret<Value> div(const Value &value) const noexcept;

        [[nodiscard]] bool equals(const Value &value) const noexcept;

        [[nodiscard]] bool discernEquals(const Value &value) const noexcept;

        [[nodiscard]] bool logicalAnd(const Value &value) const noexcept;

        [[nodiscard]] bool logicalOr(const Value &value) const noexcept;

        [[nodiscard]] Ret<bool> littlerThan(const Value &value) const noexcept;

        [[nodiscard]] Ret<bool> greaterThan(const Value &value) const noexcept;

    private:
        union {
            Integer _integer{};
            Real _real;
            String *_string;
            Octet *_octet;
            Object *_object;
        };

        ValueType _type{ ValueType::Void };

        friend std::ostream &operator<<(std::ostream &os, const Value &d);
    };

} // namespace cial

// support fmt::format
template <>
struct fmt::formatter<cial::Value> : ostream_formatter {};
