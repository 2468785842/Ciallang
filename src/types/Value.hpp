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

#include "Object.hpp"
#include "Octet.hpp"
#include "String.hpp"
#include "Types.hpp"

#include "gc/GC.hpp"

namespace Ciallang {
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

        Value operator+(const Value &tjsValue) const;

        Value operator-(const Value &tjsValue) const;

        Value operator*(const Value &tjsValue) const;

        Value operator/(const Value &tjsValue) const;

        Value operator-() const;

        bool operator==(const Value &tjsValue) const;

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

    static Value createInteger(const Integer value) { return Value{ value }; }

    static Value createReal(const Real value) { return Value{ value }; }

    template <class R, class... Args>
        requires is_gc_object_v<R>
    static Value createObject(Args... args) {
        return Value{ new R{ std::forward<Args>(args)... } };
    }

    static Value createOctet(std::uint8_t *value, std::uint32_t size) { 
        return Value{ new Octet{ value, size } }; 
    }

    template<size_t N>
    static Value createString(const char (&arr)[N]) { 
        return Value{ new String{ arr, N } }; 
    }

    static Value createString(const char *str, std::uint32_t size) { 
        return Value{ new String{ str, size } }; 
    }
    
} // namespace Ciallang

// support fmt::format
template <>
struct fmt::formatter<Ciallang::Value> : ostream_formatter {};
