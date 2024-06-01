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

#include <cassert>

#include "TjsString.hpp"
#include "TjsOctet.hpp"
#include "TjsObject.hpp"
#include "glog/logging.h"

namespace Ciallang {
    TjsValue::TjsValue(const TjsString& value) :
        _type(TjsValueType::String) {
        _value._string = new TjsString{ value };
    }

    TjsValue::TjsValue(const TjsOctet& value) :
        _type(TjsValueType::Octet) {
        _value._octet = new TjsOctet{ value };
    }

    TjsValue::TjsValue(const TjsObject& value) :
        _type(TjsValueType::Object) {
        _value._object = new TjsObject{ value };
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
            case TjsValueType::String:
                _value._string = new TjsString{ *value._value._string };
                break;
            case TjsValueType::Octet:
                _value._octet = new TjsOctet{ *value._value._octet };
                break;
            case TjsValueType::Object:
                _value._object = new TjsObject{ *value._value._object };
                break;
            case TjsValueType::Void:
                // nothing to do
                break;
        }
    }

    TjsValue::TjsValue(TjsValue&& value) noexcept {
        _type = value._type;

        switch(_type) {
            case TjsValueType::Integer:
                _value._integer = value._value._integer;
                break;
            case TjsValueType::Real:
                _value._real = value._value._real;
                break;
            case TjsValueType::String:
                _value._string = value._value._string;
                break;
            case TjsValueType::Octet:
                _value._octet = value._value._octet;
                break;
            case TjsValueType::Object:
                _value._object = value._value._object;
                break;
            case TjsValueType::Void:
                // nothing to do
                break;
        }

        value._value = {};
    }

    TjsValue& TjsValue::operator=(TjsValue&& value) noexcept {
        _type = value._type;

        switch(_type) {
            case TjsValueType::Integer:
                _value._integer = value._value._integer;
                break;
            case TjsValueType::Real:
                _value._real = value._value._real;
                break;
            case TjsValueType::String:
                _value._string = value._value._string;
                break;
            case TjsValueType::Octet:
                _value._octet = value._value._octet;
                break;
            case TjsValueType::Object:
                _value._object = value._value._object;
                break;
            case TjsValueType::Void:
                // nothing to do
                break;
        }

        value._value = {};
        return *this;
    }

    TjsValue::~TjsValue() noexcept {
        if(_type == TjsValueType::String)
            delete _value._string;
        if(_type == TjsValueType::Octet)
            delete _value._octet;
        if(_type == TjsValueType::Object)
            delete _value._object;
    }


    TjsInteger TjsValue::asInteger() const {
        CHECK(this->_type == TjsValueType::Integer) << "is not integer";
        return _value._integer;
    }

    TjsReal TjsValue::asReal() const {
        CHECK(this->_type == TjsValueType::Real) << "is not real";
        return _value._real;
    }

    TjsString* TjsValue::asString() const {
        CHECK(this->_type == TjsValueType::String) << "is not string";
        return _value._string;
    }

    TjsOctet* TjsValue::asOctet() const {
        CHECK(this->_type == TjsValueType::Octet) << "is not octet";
        return _value._octet;
    }

    TjsObject* TjsValue::asObject() const {
        CHECK(_type == TjsValueType::Object) << "is not object";
        return _value._object;
    }

    const char* TjsValue::name() const {
        return S_TypeToName.at(_type);
    }

}
