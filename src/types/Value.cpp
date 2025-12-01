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

#include "Value.hpp"

#include "logging/Logger.hpp"

namespace Ciallang {

    Value::Value(const Integer value) : _value{ ._integer = value }, _type(ValueType::Integer) {}

    Value::Value(const Real value) : _value{ ._real = value }, _type(ValueType::Real) {}

    Value::Value(String *value) : _value{ ._string = value }, _type(ValueType::String) {
        if(_value._string) {
            _value._string->incRef();
        }
    }

    Value::Value(Octet *value) : _value{ ._octet = value }, _type(ValueType::Octet) { 
        if(_value._octet) {
            _value._octet->incRef();
        }
    }

    Value::Value(Object *value) : _value{ ._object = value }, _type(ValueType::Object) {
        if(_value._object) {
            _value._object->incRef();
        }
    }

    Value::~Value() {
        switch(_type) {
            case ValueType::Object:
                if(_value._object) {
                    _value._object->decRef();
                }
                break;
            case ValueType::String:
                if(_value._string) {
                    _value._string->decRef();
                }
                break;
            case ValueType::Octet:
                if(_value._octet) {
                    _value._octet->decRef();
                }
                break;
            default:;
        }
    }

    Value::Value(const Value &v) noexcept {
        _type = v._type;

        switch(v._type) {
            case ValueType::String:
                _value._string = v._value._string;
                if(_value._string) {
                    _value._string->incRef();
                }
                break;
            case ValueType::Octet:
                _value._octet = v._value._octet;
                if(_value._octet) {
                    _value._octet->incRef();
                }
                break;
            case ValueType::Object:
                _value._object = v._value._object;
                if(_value._object) {
                    _value._object->incRef();
                }
                break;
            default:
                _value = v._value;
        }
    }

    Value &Value::operator=(const Value &value) noexcept {
        if(this != &value) {
            this->~Value();
            new(this) Value(value);
        }
        return *this;
    }

    Integer Value::toInteger() const {
        CLL_ASSERT(type() == ValueType::Integer, "not integer type is %s", name());
        return _value._integer;
    }

    Real Value::toReal() const {
        CLL_ASSERT(type() == ValueType::Real, "not real type is %s", name());
        return _value._real;
    }

    String *Value::toString() const {
        CLL_ASSERT(type() == ValueType::String, "not string type is %s", name());
        return _value._string;
    }

    Octet *Value::toOctet() const {
        CLL_ASSERT(type() == ValueType::Octet, "not octet type is %s", name());
        return _value._octet;
    }

    Object *Value::toObject() const {
        CLL_ASSERT(type() == ValueType::Object, "not object type is %s", name());
        return _value._object;
    }

    bool Value::toBool() const {
        if(type() == ValueType::Integer) {
            return toInteger() != 0;
        }
        return type() != ValueType::Void;
    }

    const char *Value::name() const {
        switch(type()) {
            case ValueType::Integer:
                return "integer";
            case ValueType::Real:
                return "real";
            case ValueType::Void:
                return "void";
            case ValueType::Object:
                return "object";
            case ValueType::String:
                return "string";
            case ValueType::Octet:
                return "octet";
            default:
                return "unknown";
        }
    }

    Value Value::operator+(const Value &tjsValue) const {
        switch(tjsValue.type()) {
            case ValueType::Integer:
                return Value{ this->toInteger() + tjsValue.toInteger() };
            case ValueType::Real:
                return Value{ this->toReal() + tjsValue.toReal() };
            default:
                throw std::logic_error("not support add operator");
        }
    }

    Value Value::operator-(const Value &tjsValue) const {
        switch(tjsValue.type()) {
            case ValueType::Integer:
                return Value{ this->toInteger() - tjsValue.toInteger() };
            case ValueType::Real:
                return Value{ this->toReal() - tjsValue.toReal() };
            default:
                throw std::logic_error("not support sub operator");
        }
    }

    Value Value::operator*(const Value &tjsValue) const {
        switch(tjsValue.type()) {
            case ValueType::Integer:
                return Value{ this->toInteger() * tjsValue.toInteger() };
            case ValueType::Real:
                return Value{ this->toReal() * tjsValue.toReal() };
            default:
                throw std::logic_error("not support mul operator");
        }
    }

    Value Value::operator/(const Value &tjsValue) const {
        switch(tjsValue.type()) {
            case ValueType::Integer:
                return Value{ this->toInteger() / tjsValue.toInteger() };
            case ValueType::Real:
                return Value{ this->toReal() / tjsValue.toReal() };
            default:
                throw std::logic_error("not support div operator");
        }
    }

    Value Value::operator-() const {
        switch(type()) {
            case ValueType::Integer:
                return Value{ -toInteger() };
            case ValueType::Real:
                return Value{ -toReal() };
            default:
                throw std::logic_error("not number!! `operator-` can't use");
        }
    }

    bool Value::operator==(const Value &tjsValue) const {
        if(type() != tjsValue.type())
            return false;
        switch(type()) {
            case ValueType::Integer:
                return toInteger() == tjsValue.toInteger();
            case ValueType::Real:
                return toReal() == tjsValue.toReal();
            case ValueType::String:
                return toString() == tjsValue.toString();
            case ValueType::Object:
                return toObject() == tjsValue.toObject();
            case ValueType::Octet:
                return toOctet() == tjsValue.toOctet();
            default:;
        }

        CLL_LOG_FATAL("not impl `==` operator in Value");
        std::abort();
    }

    std::partial_ordering Value::operator<=>(const Value &rhs) const {
        const auto t1 = type();
        const auto t2 = rhs.type();
        if(t1 == ValueType::Integer && t2 == ValueType::Integer) {
            return toInteger() <=> rhs.toInteger();
        }
        if(t1 != ValueType::String || t2 != ValueType::String) {
            return toReal() <=> rhs.toReal();
        }

        return toString() <=> rhs.toString();
    }

    std::ostream &operator<<(std::ostream &os, const Value &d) {
        switch(d.type()) {
            case ValueType::Integer:
                return os << d.toInteger();
            case ValueType::Real:
                return os << d.toReal();
            case ValueType::String:
                return os << *d.toString();
            case ValueType::Octet:
                throw std::logic_error("not support");
            case ValueType::Object:
                return os << "<object>[" << d.toObject()->name() << ']';
            case ValueType::Void:
                return os << "void";
        }
        return os << "unknown";
    }
} // namespace Ciallang
