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

#include <stdexcept>
#include <unordered_map>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include "TjsTypes.hpp"
#include "TjsString.hpp"
#include "TjsOctet.hpp"
#include "TjsObject.hpp"

namespace Ciallang {
    class TjsValue {
    public:
        TjsValue() = default;

        explicit TjsValue(const TjsInteger& value) : _value{ ._integer = value }, _type(TjsValueType::Integer) {
        }

        explicit TjsValue(const TjsReal& value) : _value{ ._real = value }, _type(TjsValueType::Real) {
        }

        explicit TjsValue(const TjsString&);

        explicit TjsValue(const TjsOctet&);

        explicit TjsValue(const TjsObject&);

        TjsValue(const TjsValue&) noexcept;

        TjsValue(TjsValue&&) noexcept;

        TjsValue& operator=(TjsValue&& value) noexcept;

        ~TjsValue() noexcept;

        [[nodiscard]] constexpr TjsValueType type() const noexcept { return _type; }

        constexpr void type(const TjsValueType type) noexcept { _type = type; }

        [[nodiscard]] TjsInteger asInteger() const;

        [[nodiscard]] TjsReal asReal() const;

        [[nodiscard]] TjsString* asString() const;

        [[nodiscard]] TjsOctet* asOctet() const;

        [[nodiscard]] TjsObject* asObject() const;

        [[nodiscard]] const char* name() const;

    private:
        union {
            TjsInteger _integer;
            TjsReal _real;
            TjsString* _string;
            TjsOctet* _octet;
            TjsObject* _object;
        } _value{};

        TjsValueType _type{ TjsValueType::Void };

        friend std::ostream& operator<<(std::ostream& os, const TjsValue& d) {
            switch(d.type()) {
                case TjsValueType::Integer:
                    return os << d.asInteger();
                case TjsValueType::Real:
                    return os << d.asReal();
                case TjsValueType::String:
                    return os << d.asString();
                case TjsValueType::Octet:
                case TjsValueType::Object:
                    throw std::logic_error("not support");
                case TjsValueType::Void:
                    return os << "void";
            }
            return os << "unknown";
        }
    };

    static const std::unordered_map<TjsValueType, const char*> S_TypeToName{
            { TjsValueType::Void, "Void" },
            { TjsValueType::Object, "Object" },
            { TjsValueType::String, "String" },
            { TjsValueType::Octet, "Octet" }, // octet binary data
            { TjsValueType::Integer, "Integer" },
            { TjsValueType::Real, "Real" }
    };

    static TjsValue tjsInteger(const TjsInteger value) {
        return TjsValue{ value };
    }

    static TjsValue tjsReal(const TjsReal value) {
        return TjsValue{ value };
    }
} // namespace Ciallang

// support fmt::format
template <>
struct fmt::formatter<Ciallang::TjsValue> : ostream_formatter {
};
