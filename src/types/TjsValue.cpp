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

#include "TjsOctet.hpp"
#include "TjsObject.hpp"


namespace Ciallang {
    TjsValue::TjsValue(const TjsInteger& value) :
        _value{ ._integer = value },
        _type(TjsValueType::Integer) {
    }

    TjsValue::TjsValue(const TjsReal& value) :
        _value{ ._real = value },
        _type(TjsValueType::Real) {
    }

    TjsValue::TjsValue(const std::string& value) :
        _type(TjsValueType::String) {
        _value._string = new std::string{ value };
    }

    TjsValue::TjsValue(const TjsOctet& value) :
        _type(TjsValueType::Octet) {
        _value._octet = new TjsOctet{ value };
    }

    TjsValue::TjsValue(const TjsValue& value) noexcept {
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
                _value._string = new std::string(*value._value._string);
                break;
            case TjsValueType::Octet:
                _value._octet = new TjsOctet(*value._value._octet);
                break;
            default: ;
        }
    }

    TjsValue::TjsValue(TjsValue&& value) noexcept :
        _type(value._type),
        _value(value._value) {
        value._value = {};
    }

    TjsValue& TjsValue::operator=(TjsValue&& value) noexcept {
        if(this == &value) return *this;

        switch(_type) {
        case TjsValueType::Object:
            // TODO: GC.
            break;
        case TjsValueType::String:
            delete _value._string;
            break;
        case TjsValueType::Octet:
            delete _value._octet;
            break;
        default: ;
        }

        _value = value._value;
        _type = value._type;
        return *this;
    }

    TjsValue::~TjsValue() noexcept {
        switch(_type) {
            case TjsValueType::Object:                 
               // TODO: add GC 
                break;
            case TjsValueType::String: delete _value._string;
                break;
            case TjsValueType::Octet: delete _value._octet;
                break;
            default: ;
        }
    }

    TjsInteger TjsValue::asInteger() const {
        CHECK(this->_type == TjsValueType::Integer)
        << "is not integer " << "is " << name();
        return _value._integer;
    }

    TjsReal TjsValue::asReal() const {
        CHECK(this->_type == TjsValueType::Real)
        << "is not real " << "is " << name();
        return _value._real;
    }

    std::string* TjsValue::asString() const {
        CHECK(this->_type == TjsValueType::String)
        << "is not string " << "is " << name();
        return _value._string;
    }

    TjsOctet* TjsValue::asOctet() const {
        CHECK(this->_type == TjsValueType::Octet)
        << "is not octet " << "is " << name();
        return _value._octet;
    }

    TjsObject* TjsValue::asObject() const {
        CHECK(_type == TjsValueType::Object)
        << "is not object " << "is " << name();
        return _value._object;
    }

    bool TjsValue::asBool() const {
        if(_type == TjsValueType::Integer) {
            return asInteger() != 0;
        }
        return _type != TjsValueType::Void;
    }

    std::string TjsValue::name() const {
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
        }
        return "unknown";
    }

    TjsValue TjsValue::operator+(const TjsValue& tjsValue) const {
        switch(tjsValue.type()) {
            case TjsValueType::Integer:
                return TjsValue{ this->asInteger() + tjsValue.asInteger() };
            case TjsValueType::Real:
                return TjsValue{ this->asReal() + tjsValue.asReal() };
            default:
                throw std::logic_error("not support add operator");
        }
    }

    TjsValue TjsValue::operator-(const TjsValue& tjsValue) const {
        switch(tjsValue.type()) {
            case TjsValueType::Integer:
                return TjsValue{ this->asInteger() - tjsValue.asInteger() };
            case TjsValueType::Real:
                return TjsValue{ this->asReal() - tjsValue.asReal() };
            default:
                throw std::logic_error("not support sub operator");
        }
    }

    TjsValue TjsValue::operator*(const TjsValue& tjsValue) const {
        switch(tjsValue.type()) {
            case TjsValueType::Integer:
                return TjsValue{ this->asInteger() * tjsValue.asInteger() };
            case TjsValueType::Real:
                return TjsValue{ this->asReal() * tjsValue.asReal() };
            default:
                throw std::logic_error("not support mul operator");
        }
    }

    TjsValue TjsValue::operator/(const TjsValue& tjsValue) const {
        switch(tjsValue.type()) {
            case TjsValueType::Integer:
                return TjsValue{ this->asInteger() / tjsValue.asInteger() };
            case TjsValueType::Real:
                return TjsValue{ this->asReal() / tjsValue.asReal() };
            default:
                throw std::logic_error("not support div operator");
        }
    }

    TjsValue TjsValue::operator-() const {
        switch(type()) {
            case TjsValueType::Integer:
                return TjsValue{ -asInteger() };
            case TjsValueType::Real:
                return TjsValue{ -asReal() };
            default:
                throw std::logic_error("not number!! `operator-` can't use");
        }
    }

    bool TjsValue::operator==(const TjsValue& tjsValue) const {
        if(type() != tjsValue.type()) return false;
        switch(type()) {
            case TjsValueType::Integer:
                return asInteger() == tjsValue.asInteger();
            case TjsValueType::Real:
                return asReal() == tjsValue.asReal();
            case TjsValueType::String:
                return *asString() == *tjsValue.asString();
            case TjsValueType::Object:
                return asObject() == tjsValue.asObject();
            case TjsValueType::Octet:
            default: ;
        }

        LOG(FATAL) << "no impl == funcition in TjsValue";
        std::abort();
    }

    std::partial_ordering TjsValue::operator<=>(const TjsValue& tjsValue) const {
        if(_type == TjsValueType::Integer
           && tjsValue._type == TjsValueType::Integer) {
            return asInteger() <=> tjsValue.asInteger();
        }
        if(_type != TjsValueType::String
           || tjsValue._type != TjsValueType::String) {
            return asReal() <=> tjsValue.asReal();
        }
        return asString() <=> tjsValue.asString();
    }

    std::ostream& operator<<(std::ostream& os, const TjsValue& d) {
        switch(d.type()) {
            case TjsValueType::Integer:
                return os << d.asInteger();
            case TjsValueType::Real:
                return os << d.asReal();
            case TjsValueType::String:
                return os << *d.asString();
            case TjsValueType::Octet:
                throw std::logic_error("not support");
            case TjsValueType::Object:
                return os << "<object>[" << d.asObject()->name() << ']';
            case TjsValueType::Void:
                return os << "void";
        }
        return os << "unknown";
    }
}
