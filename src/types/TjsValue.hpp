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

#include "pch.h"

#include "TjsTypes.hpp"

namespace Ciallang {
    class TjsValue {
        friend struct TjsValueHelper;

    public:
        TjsValue() = default;

        explicit TjsValue(const TjsInteger& value);

        explicit TjsValue(const TjsReal& value);

        explicit TjsValue(const std::string&);

        explicit TjsValue(const TjsOctet&);

        template <typename T>
            requires std::is_base_of_v<TjsObject, T>
        explicit TjsValue(const T& value) :
            _type(TjsValueType::Object) {
            _value._object = new T{ value };
        }

        TjsValue(const TjsValue& value) noexcept;

        TjsValue(TjsValue&&) noexcept;

        TjsValue& operator=(const TjsValue& value) = delete;

        TjsValue& operator=(TjsValue&& value) noexcept;

        ~TjsValue() noexcept;

        [[nodiscard]] constexpr TjsValueType type() const noexcept { return _type; }

        constexpr void type(const TjsValueType type) noexcept { _type = type; }

        [[nodiscard]] TjsInteger asInteger() const;

        [[nodiscard]] TjsReal asReal() const;

        [[nodiscard]] std::string* asString() const;

        [[nodiscard]] TjsOctet* asOctet() const;

        [[nodiscard]] TjsObject* asObject() const;

        [[nodiscard]] bool isVoid() const {
            return _type == TjsValueType::Void;
        }

        [[nodiscard]] bool isInteger() const {
            return _type == TjsValueType::Integer;
        }

        [[nodiscard]] bool isReal() const {
            return _type == TjsValueType::Real;
        }

        [[nodiscard]] bool isString() const {
            return _type == TjsValueType::String;
        }

        [[nodiscard]] bool isOctet() const {
            return _type == TjsValueType::String;
        }

        [[nodiscard]] bool isObject() const {
            return _type == TjsValueType::Object;
        }

        [[nodiscard]] bool asBool() const;

        [[nodiscard]] std::string name() const;

        TjsValue operator+(const TjsValue& tjsValue) const;

        TjsValue operator-(const TjsValue& tjsValue) const;

        TjsValue operator*(const TjsValue& tjsValue) const;

        TjsValue operator/(const TjsValue& tjsValue) const;

        TjsValue operator-() const;

        bool operator==(const TjsValue& tjsValue) const;

        std::partial_ordering operator<=>(const TjsValue& tjsValue) const;

    private:
        union {
            TjsInteger _integer;
            TjsReal _real;
            std::string* _string;
            TjsOctet* _octet;
            TjsObject* _object;
        } _value{};

        TjsValueType _type{ TjsValueType::Void };

        friend std::ostream& operator<<(std::ostream& os, const TjsValue& d);
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
