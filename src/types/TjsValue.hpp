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

#include "TjsTypes.hpp"
#include "Object.hpp"
#include "TjsOctet.hpp"
#include "TjsString.hpp"

#include "gc/GC.hpp"

namespace Ciallang {
    class TjsValue {

    public:
        TjsValue() = default;

        explicit TjsValue(TjsInteger value);

        explicit TjsValue(TjsReal value);

        explicit TjsValue(const TjsString &value);

        explicit TjsValue(const TjsOctet &value);

        explicit TjsValue(Object *value);

        ~TjsValue();

        TjsValue(const TjsValue &v) noexcept;
        TjsValue(TjsValue &&) noexcept;

        TjsValue &operator=(const TjsValue &value) noexcept;

        TjsValue &operator=(TjsValue &&rhs) noexcept;

        [[nodiscard]] TjsValueType type() const noexcept { return _type; }

        void type(const TjsValueType type) noexcept { _type = type; }

        [[nodiscard]] TjsInteger toInteger() const;

        [[nodiscard]] TjsReal toReal() const;

        [[nodiscard]] TjsString *toString() const;

        [[nodiscard]] TjsOctet *toOctet() const;

        [[nodiscard]] Object *toObject() const;

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

        std::partial_ordering operator<=>(const TjsValue &rhs) const;

    private:
        union {
            TjsInteger _integer;
            TjsReal _real;
            TjsString *_string;
            TjsOctet *_octet;
            Object *_object;
        } _value{};

        TjsValueType _type{ TjsValueType::Void };

        friend std::ostream &operator<<(std::ostream &os, const TjsValue &d);
    };

    static TjsValue tjsInteger(const TjsInteger value) { return TjsValue{ value }; }

    static TjsValue tjsReal(const TjsReal value) { return TjsValue{ value }; }

    template<class R, class... Args>
    requires is_gc_object_v<R>
    static TjsValue tjsObject(Args... args) { return TjsValue{ new R { std::forward<Args>(args)... } }; }
} // namespace Ciallang

// support fmt::format
template <>
struct fmt::formatter<Ciallang::TjsValue> : ostream_formatter {};
