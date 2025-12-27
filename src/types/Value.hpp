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

namespace Cial {
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

        [[nodiscard]] Integer toInteger() const;

        [[nodiscard]] Real toReal() const;

        [[nodiscard]] String *toString() const;

        [[nodiscard]] Octet *toOctet() const;

        [[nodiscard]] Object *toObject() const;

        [[nodiscard]] bool isVoid() const { return type() == ValueType::Void; }

        [[nodiscard]] bool isInteger() const { return type() == ValueType::Integer; }

        [[nodiscard]] bool isReal() const { return type() == ValueType::Real; }

        [[nodiscard]] bool isString() const { return type() == ValueType::String; }

        [[nodiscard]] bool isOctet() const { return type() == ValueType::Octet; }

        [[nodiscard]] bool isObject() const { return type() == ValueType::Object; }

        [[nodiscard]] bool toBool() const;

        [[nodiscard]] const char *name() const;

        Value operator+(const Value &value) const;

        Value operator-(const Value &value) const;

        Value operator*(const Value &value) const;

        Value operator/(const Value &value) const;

        Value operator-() const;

        [[nodiscard]] bool discernCompare(const Value &value) const;
        bool operator==(const Value &value) const;

        std::partial_ordering operator<=>(const Value &rhs) const;

    private:
        union {
            Integer _integer;
            Real _real;
            String *_string;
            Octet *_octet;
            Object *_object;
        } _value{};

        ValueType _type{ ValueType::Void };

        friend std::ostream &operator<<(std::ostream &os, const Value &d);
    };

} // namespace Cial

// support fmt::format
template <>
struct fmt::formatter<Cial::Value> : ostream_formatter {};
