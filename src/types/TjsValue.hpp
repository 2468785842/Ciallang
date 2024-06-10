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

#include "TjsTypes.hpp"

namespace Ciallang {
    class TjsValue {
        template <Common::Name>
        friend struct MakeTjsValueHelper;

    public:
        TjsValue() = default;

        explicit TjsValue(const TjsInteger& value) :
            _value{ ._integer = value },
            _type(TjsValueType::Integer) {
        }

        explicit TjsValue(const TjsReal& value) :
            _value{ ._real = value },
            _type(TjsValueType::Real) {
        }

        explicit TjsValue(const TjsString&);

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

        [[nodiscard]] TjsString* asString() const;

        [[nodiscard]] TjsOctet* asOctet() const;

        [[nodiscard]] TjsObject* asObject() const;

        [[nodiscard]] bool asBool() const;

        [[nodiscard]] const char* name() const;

        TjsValue operator+(const TjsValue& tjsValue) const {
            switch(tjsValue.type()) {
                case TjsValueType::Integer:
                    return TjsValue{ this->asInteger() + tjsValue.asInteger() };
                case TjsValueType::Real:
                    return TjsValue{ this->asReal() + tjsValue.asReal() };
                default:
                    throw std::logic_error("not support add operator");
            }
        }

        TjsValue operator-(const TjsValue& tjsValue) const {
            switch(tjsValue.type()) {
                case TjsValueType::Integer:
                    return TjsValue{ this->asInteger() - tjsValue.asInteger() };
                case TjsValueType::Real:
                    return TjsValue{ this->asReal() - tjsValue.asReal() };
                default:
                    throw std::logic_error("not support sub operator");
            }
        }

        TjsValue operator*(const TjsValue& tjsValue) const {
            switch(tjsValue.type()) {
                case TjsValueType::Integer:
                    return TjsValue{ this->asInteger() * tjsValue.asInteger() };
                case TjsValueType::Real:
                    return TjsValue{ this->asReal() * tjsValue.asReal() };
                default:
                    throw std::logic_error("not support mul operator");
            }
        }

        TjsValue operator/(const TjsValue& tjsValue) const {
            switch(tjsValue.type()) {
                case TjsValueType::Integer:
                    return TjsValue{ this->asInteger() / tjsValue.asInteger() };
                case TjsValueType::Real:
                    return TjsValue{ this->asReal() / tjsValue.asReal() };
                default:
                    throw std::logic_error("not support div operator");
            }
        }

        TjsValue operator-() const {
            switch(type()) {
                case TjsValueType::Integer:
                    return TjsValue{ -asInteger() };
                case TjsValueType::Real:
                    return TjsValue{ -asReal() };
                default:
                    throw std::logic_error("not number!! `operator-` can't use");
            }
        }

        bool operator==(const TjsValue& tjsValue) const;

    private:
        union {
            TjsInteger _integer;
            TjsReal _real;
            TjsString* _string;
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

    template <>
    inline void TjsIntegerHelper::copy(const TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
        dest._value._integer = src._value._integer;
    }

    template <>
    inline void TjsIntegerHelper::move(TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
        dest._value._integer = src._value._integer;
    }

    template <>
    inline void TjsIntegerHelper::destroy(TjsValue&) const {
        // nothing to do
    }


    template <>
    inline void TjsRealHelper::copy(const TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
        dest._value._real = src._value._real;
    }

    template <>
    inline void TjsRealHelper::move(TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
        dest._value._real = src._value._real;
    }

    template <>
    inline void TjsRealHelper::destroy(TjsValue&) const {
        // nothing to do
    }

    template <>
    inline void TjsVoidHelper::copy(const TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
    }

    template <>
    inline void TjsVoidHelper::move(TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
    }

    template <>
    inline void TjsVoidHelper::destroy(TjsValue&) const {
        // nothing to do
    }
} // namespace Ciallang

// support fmt::format
template <>
struct fmt::formatter<Ciallang::TjsValue> : ostream_formatter {
};
