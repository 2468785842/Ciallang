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

#include "TjsString.hpp"
#include "TjsOctet.hpp"
#include "TjsObject.hpp"


namespace Ciallang {
    TjsValue::TjsValue(const TjsString& value) :
        _type(TjsValueType::String) {
        _value._string = new TjsString{ value };
    }

    TjsValue::TjsValue(const TjsOctet& value) :
        _type(TjsValueType::Octet) {
        _value._octet = new TjsOctet{ value };
    }

    TjsValue::TjsValue(const TjsValue& value) noexcept {
        TjsValueHelper::instance(value._type)->copy(value, *this);
    }

    TjsValue::TjsValue(TjsValue&& value) noexcept {
        TjsValueHelper::instance(value._type)->move(value, *this);
    }

    TjsValue& TjsValue::operator=(TjsValue&& value) noexcept {
        if(this != &value) {
            TjsValueHelper::instance(this->_type)->destroy(*this);
            TjsValueHelper::instance(value._type)->move(value, *this);
        }
        return *this;
    }

    TjsValue::~TjsValue() noexcept {
        TjsValueHelper::instance(_type)->destroy(*this);
    }

    TjsInteger TjsValue::asInteger() const {
        CHECK(this->_type == TjsValueType::Integer)
        << "is not integer" << "is " << name();
        return _value._integer;
    }

    TjsReal TjsValue::asReal() const {
        CHECK(this->_type == TjsValueType::Real)
        << "is not real" << "is " << name();
        return _value._real;
    }

    TjsString* TjsValue::asString() const {
        CHECK(this->_type == TjsValueType::String)
        << "is not string" << "is " << name();
        return _value._string;
    }

    TjsOctet* TjsValue::asOctet() const {
        CHECK(this->_type == TjsValueType::Octet)
        << "is not octet" << "is " << name();
        return _value._octet;
    }


    TjsObject* TjsValue::asObject() const {
        CHECK(_type == TjsValueType::Object)
        << "is not object" << "is " << name();
        return _value._object;
    }

    bool TjsValue::asBool() const {
        if(_type == TjsValueType::Integer) {
            return asInteger() != 0;
        }
        return _type != TjsValueType::Void;
    }

    const char* TjsValue::name() const {
        return TjsValueHelper::instance(_type)->name();
    }


    bool TjsValue::operator==(const TjsValue& tjsValue) const {
        if(type() != tjsValue.type()) return false;
        switch(type()) {
            case TjsValueType::Integer:
                return asInteger() == tjsValue.asInteger();
            case TjsValueType::Real:
                return asReal() == tjsValue.asReal();
            case TjsValueType::String:
                return asString() == tjsValue.asString();
            case TjsValueType::Object:
            // return asObject() == asObject();
            case TjsValueType::Octet:
            default: ;
        }

        LOG(FATAL) << "no impl == funcition in TjsValue";
        std::abort();
    }

    std::ostream& operator<<(std::ostream& os, const TjsValue& d) {
        switch(d.type()) {
            case TjsValueType::Integer:
                return os << d.asInteger();
            case TjsValueType::Real:
                return os << d.asReal();
            case TjsValueType::String:
                return os << d.asString();
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
