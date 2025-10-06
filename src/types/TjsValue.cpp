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

#include "TjsValue.hpp"

#include "TjsObject.hpp"
#include "TjsOctet.hpp"
#include "TjsString.hpp"
#include "logging/Logger.hpp"

namespace Ciallang {
    TjsValue::TjsValue(const TjsInteger &value) : _value{ ._integer = value }, _type(TjsValueType::Integer) {}

    TjsValue::TjsValue(const TjsReal &value) : _value{ ._real = value }, _type(TjsValueType::Real) {}

    TjsValue::TjsValue(const TjsString &value) : _type(TjsValueType::String) {
        _value._string = new TjsString{ value };
    }

    TjsValue::TjsValue(const TjsOctet &value) : _type(TjsValueType::Octet) { _value._octet = new TjsOctet{ value }; }

    TjsValue::TjsValue(const TjsValue &value) noexcept {
        _type = value._type;
        switch(_type) {
            case TjsValueType::Integer:
                _value._integer = value._value._integer;
                break;
            case TjsValueType::Real:
                _value._real = value._value._real;
                break;
            case TjsValueType::Object:
                _value._object = value._value._object;
                break;
            case TjsValueType::String:
                _value._string = new TjsString(*value._value._string);
                break;
            case TjsValueType::Octet:
                _value._octet = new TjsOctet(*value._value._octet);
                break;
            default:;
        }
    }

    TjsValue::TjsValue(TjsValue &&value) noexcept : _value(value._value), _type(value._type) { value._value = {}; }

    TjsValue &TjsValue::operator=(TjsValue &&value) noexcept {
        if(this == &value)
            return *this;

        _value = value._value;
        _type = value._type;
        value._value = {};
        value._type = TjsValueType::Void;
        return *this;
    }

    TjsValue::~TjsValue() noexcept {
        switch(_type) {
            case TjsValueType::Object:
                // TODO: add GC
                break;
            case TjsValueType::String:
                delete _value._string;
                break;
            case TjsValueType::Octet:
                delete _value._octet;
                break;
            default:;
        }
    }

    TjsInteger TjsValue::toInteger() const {
        CLL_ASSERT(_type == TjsValueType::Integer, "not integer type is %s", name());
        return _value._integer;
    }

    TjsReal TjsValue::toReal() const {
        CLL_ASSERT(_type == TjsValueType::Real, "not real type is %s", name());
        return _value._real;
    }

    TjsString *TjsValue::toString() const {
        CLL_ASSERT(_type == TjsValueType::String, "not string type is %s", name());
        return _value._string;
    }

    TjsOctet *TjsValue::toOctet() const {
        CLL_ASSERT(_type == TjsValueType::Octet, "not octet type is %s", name());
        return _value._octet;
    }

    TjsObject *TjsValue::toObject() const {
        CLL_ASSERT(_type == TjsValueType::Object, "not object type is %s", name());
        return _value._object;
    }

    bool TjsValue::toBool() const {
        if(_type == TjsValueType::Integer) {
            return toInteger() != 0;
        }
        return _type != TjsValueType::Void;
    }

    const char *TjsValue::name() const {
        switch(_type) {
            case TjsValueType::Integer:
                return "integer";
            case TjsValueType::Real:
                return "real";
            case TjsValueType::Void:
                return "void";
            case TjsValueType::Object:
                return "object";
            case TjsValueType::String:
                return "string";
            case TjsValueType::Octet:
                return "octet";
            default:
                return "unknown";
        }
    }

    TjsValue TjsValue::operator+(const TjsValue &tjsValue) const {
        switch(tjsValue.type()) {
            case TjsValueType::Integer:
                return TjsValue{ this->toInteger() + tjsValue.toInteger() };
            case TjsValueType::Real:
                return TjsValue{ this->toReal() + tjsValue.toReal() };
            default:
                throw std::logic_error("not support add operator");
        }
    }

    TjsValue TjsValue::operator-(const TjsValue &tjsValue) const {
        switch(tjsValue.type()) {
            case TjsValueType::Integer:
                return TjsValue{ this->toInteger() - tjsValue.toInteger() };
            case TjsValueType::Real:
                return TjsValue{ this->toReal() - tjsValue.toReal() };
            default:
                throw std::logic_error("not support sub operator");
        }
    }

    TjsValue TjsValue::operator*(const TjsValue &tjsValue) const {
        switch(tjsValue.type()) {
            case TjsValueType::Integer:
                return TjsValue{ this->toInteger() * tjsValue.toInteger() };
            case TjsValueType::Real:
                return TjsValue{ this->toReal() * tjsValue.toReal() };
            default:
                throw std::logic_error("not support mul operator");
        }
    }

    TjsValue TjsValue::operator/(const TjsValue &tjsValue) const {
        switch(tjsValue.type()) {
            case TjsValueType::Integer:
                return TjsValue{ this->toInteger() / tjsValue.toInteger() };
            case TjsValueType::Real:
                return TjsValue{ this->toReal() / tjsValue.toReal() };
            default:
                throw std::logic_error("not support div operator");
        }
    }

    TjsValue TjsValue::operator-() const {
        switch(type()) {
            case TjsValueType::Integer:
                return TjsValue{ -toInteger() };
            case TjsValueType::Real:
                return TjsValue{ -toReal() };
            default:
                throw std::logic_error("not number!! `operator-` can't use");
        }
    }

    bool TjsValue::operator==(const TjsValue &tjsValue) const {
        if(type() != tjsValue.type())
            return false;
        switch(type()) {
            case TjsValueType::Integer:
                return toInteger() == tjsValue.toInteger();
            case TjsValueType::Real:
                return toReal() == tjsValue.toReal();
            case TjsValueType::String:
                return *toString() == *tjsValue.toString();
            case TjsValueType::Object:
                return toObject() == tjsValue.toObject();
            case TjsValueType::Octet:
            default:;
        }

        CLL_LOG_FATAL("not impl `==` operator in TjsValue");
        std::abort();
    }

    std::partial_ordering TjsValue::operator<=>(const TjsValue &tjsValue) const {
        if(_type == TjsValueType::Integer && tjsValue._type == TjsValueType::Integer) {
            return toInteger() <=> tjsValue.toInteger();
        }
        if(_type != TjsValueType::String || tjsValue._type != TjsValueType::String) {
            return toReal() <=> tjsValue.toReal();
        }
        return toString() <=> tjsValue.toString();
    }

    std::ostream &operator<<(std::ostream &os, const TjsValue &d) {
        switch(d.type()) {
            case TjsValueType::Integer:
                return os << d.toInteger();
            case TjsValueType::Real:
                return os << d.toReal();
            case TjsValueType::String:
                return os << *d.toString();
            case TjsValueType::Octet:
                throw std::logic_error("not support");
            case TjsValueType::Object:
                return os << "<object>[" << d.toObject()->name() << ']';
            case TjsValueType::Void:
                return os << "void";
        }
        return os << "unknown";
    }
} // namespace Ciallang
