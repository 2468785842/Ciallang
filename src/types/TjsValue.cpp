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

#include "logging/Logger.hpp"

namespace Ciallang {

    TjsValue::TjsValue(const TjsInteger value) : _value{ ._integer = value }, _type(TjsValueType::Integer) {}

    TjsValue::TjsValue(const TjsReal value) : _value{ ._real = value }, _type(TjsValueType::Real) {}

    TjsValue::TjsValue(const TjsString &value) : _type(TjsValueType::String) {
        _value._string = new TjsString{ value };
    }

    TjsValue::TjsValue(const TjsOctet &value) : _type(TjsValueType::Octet) { _value._octet = new TjsOctet{ value }; }

    TjsValue::TjsValue(Object *value) : _value{ ._object = value }, _type(TjsValueType::Object) {
        if (_value._object) {
            _value._object->incRef();
        }
    }

    TjsValue::~TjsValue() {
        switch(_type) {
            case TjsValueType::Object:
                if (_value._object) {
                    _value._object->decRef();
                }
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

    TjsValue::TjsValue(const TjsValue &v) noexcept {
        _type = v._type;

        switch(v._type) {
            case TjsValueType::String:
                delete _value._string;
                _value._string = new TjsString(*v._value._string);
                break;
            case TjsValueType::Octet:
                delete _value._octet;
                _value._octet = new TjsOctet(*v._value._octet);
                break;
            case TjsValueType::Object:
                _value._object = v._value._object;
                if (_value._object) {
                    _value._object->incRef();
                }
                break;
            default:
                _value = v._value;
        }
    }

    TjsValue& TjsValue::operator=(const TjsValue &value) noexcept {
        if(this != &value) {
            this->~TjsValue();
            new(this) TjsValue(value);
        }
        return *this;
    }

    TjsValue::TjsValue(TjsValue &&rhs) noexcept : _value(rhs._value), _type(rhs._type) {
        rhs._type = TjsValueType::Void;
    }

    TjsValue &TjsValue::operator=(TjsValue &&rhs) noexcept {
        if(this != &rhs) {
            this->~TjsValue();
            new(this) TjsValue(std::move(rhs));
        }
        return *this;
    }

    TjsInteger TjsValue::toInteger() const {
        CLL_ASSERT(type() == TjsValueType::Integer, "not integer type is %s", name());
        return _value._integer;
    }

    TjsReal TjsValue::toReal() const {
        CLL_ASSERT(type() == TjsValueType::Real, "not real type is %s", name());
        return _value._real;
    }

    TjsString *TjsValue::toString() const {
        CLL_ASSERT(type() == TjsValueType::String, "not string type is %s", name());
        return _value._string;
    }

    TjsOctet *TjsValue::toOctet() const {
        CLL_ASSERT(type() == TjsValueType::Octet, "not octet type is %s", name());
        return _value._octet;
    }

    Object *TjsValue::toObject() const {
        CLL_ASSERT(type() == TjsValueType::Object, "not object type is %s", name());
        return _value._object;
    }

    bool TjsValue::toBool() const {
        if(type() == TjsValueType::Integer) {
            return toInteger() != 0;
        }
        return type() != TjsValueType::Void;
    }

    const char *TjsValue::name() const {
        switch(type()) {
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

    std::partial_ordering TjsValue::operator<=>(const TjsValue &rhs) const {
        const auto t1 = type();
        const auto t2 = rhs.type();
        if(t1 == TjsValueType::Integer && t2 == TjsValueType::Integer) {
            return toInteger() <=> rhs.toInteger();
        }
        if(t1 != TjsValueType::String || t2 != TjsValueType::String) {
            return toReal() <=> rhs.toReal();
        }

        return toString() <=> rhs.toString();
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
