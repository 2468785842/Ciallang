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
        struct Helper {
            union {
                TjsInteger integer;
                TjsReal real;
                TjsString *string;
                TjsOctet *octet;
                TjsObject *object;
            } value{};

            TjsValueType type{ TjsValueType::Void };
            explicit Helper() = default;

            explicit Helper(const TjsInteger value) : type(TjsValueType::Integer) { this->value.integer = value; }

            explicit Helper(const TjsReal value) : type(TjsValueType::Real) { this->value.real = value; }

            explicit Helper(TjsString *value) : type(TjsValueType::String) { this->value.string = value; }

            explicit Helper(TjsOctet *value) : type(TjsValueType::Octet) { this->value.octet = value; }

            explicit Helper(TjsObject *value) : type(TjsValueType::Object) { this->value.object = value; }

            Helper(const Helper &other) = delete;

            Helper(Helper &&other) noexcept {
                type = other.type;
                value = other.value;
                other.type = TjsValueType::Void;
            }

            Helper &operator=(const Helper &other) = delete;

            Helper &operator=(Helper &&other) noexcept {
                if (this == &other) return *this;
                this->~Helper();
                new (this) Helper(std::move(other));
                return *this;
            }

            ~Helper();
        };

    public:
        TjsValue() = default;

        explicit TjsValue(TjsInteger value);

        explicit TjsValue(TjsReal value);

        explicit TjsValue(const TjsString &value);

        explicit TjsValue(const TjsOctet &value);

        explicit TjsValue(TjsObject *value);

        TjsValue(const TjsValue &value) noexcept;

        TjsValue(TjsValue &&) = default;

        TjsValue &operator=(const TjsValue &value) = delete;

        TjsValue &operator=(TjsValue &&value) = default;

        [[nodiscard]] TjsValueType type() const noexcept { return _helper.type; }

        void type(const TjsValueType type) noexcept { _helper.type = type; }

        [[nodiscard]] TjsInteger toInteger() const;

        [[nodiscard]] TjsReal toReal() const;

        [[nodiscard]] TjsString *toString() const;

        [[nodiscard]] TjsOctet *toOctet() const;

        [[nodiscard]] TjsObject *toObject() const;

        [[nodiscard]] bool isVoid() const { return type() == TjsValueType::Void; }

        [[nodiscard]] bool isInteger() const { return type() == TjsValueType::Integer; }

        [[nodiscard]] bool isReal() const { return type() == TjsValueType::Real; }

        [[nodiscard]] bool isString() const { return type() == TjsValueType::String; }

        [[nodiscard]] bool isOctet() const { return type() == TjsValueType::Octet; }

        [[nodiscard]] bool isObject() const { return type() == TjsValueType::Object; }

        [[nodiscard]] bool toBool() const;

        [[nodiscard]] const char *name() const;

        TjsValue operator+(const TjsValue &tjsValue) const;

        TjsValue operator-(const TjsValue &tjsValue) const;

        TjsValue operator*(const TjsValue &tjsValue) const;

        TjsValue operator/(const TjsValue &tjsValue) const;

        TjsValue operator-() const;

        bool operator==(const TjsValue &tjsValue) const;

        std::partial_ordering operator<=>(const TjsValue &tjsValue) const;

    private:
        Helper _helper{};

        friend std::ostream &operator<<(std::ostream &os, const TjsValue &d);
    };

    static TjsValue tjsInteger(const TjsInteger value) { return TjsValue{ value }; }

    static TjsValue tjsReal(const TjsReal value) { return TjsValue{ value }; }
} // namespace Ciallang

// support fmt::format
template <>
struct fmt::formatter<Ciallang::TjsValue> : ostream_formatter {};
