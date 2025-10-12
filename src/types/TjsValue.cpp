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

    TjsValue::Helper::~Helper() {
        switch(type) {
            case TjsValueType::Object:
                // TODO: add GC
                break;
            case TjsValueType::String:
                delete value.string;
                break;
            case TjsValueType::Octet:
                delete value.octet;
                break;
            default:;
        }
    }

    TjsValue::TjsValue(const TjsInteger value) : _helper(value) {}

    TjsValue::TjsValue(const TjsReal value) : _helper(value) {}

    TjsValue::TjsValue(const TjsString &value) : _helper(new TjsString{ value }) {}

    TjsValue::TjsValue(const TjsOctet &value) : _helper(new TjsOctet{ value }) {}

    TjsValue::TjsValue(TjsObject *value) : _helper(value) {}

    TjsValue::TjsValue(const TjsValue &value) noexcept {
        switch(value._helper.type) {
            case TjsValueType::Integer:
                _helper = Helper(value._helper.value.integer);
                break;
            case TjsValueType::Real:
                _helper = Helper(value._helper.value.real);
                break;
            case TjsValueType::Object:
                _helper = Helper(value._helper.value.object);
                break;
            case TjsValueType::String:
                _helper = Helper(new TjsString(*value._helper.value.string));
                break;
            case TjsValueType::Octet:
                _helper = Helper(new TjsOctet(*value._helper.value.octet));
                break;
            default:;
        }
    }

    TjsInteger TjsValue::toInteger() const {
        CLL_ASSERT(type() == TjsValueType::Integer, "not integer type is %s", name());
        return _helper.value.integer;
    }

    TjsReal TjsValue::toReal() const {
        CLL_ASSERT(type() == TjsValueType::Real, "not real type is %s", name());
        return _helper.value.real;
    }

    TjsString *TjsValue::toString() const {
        CLL_ASSERT(type() == TjsValueType::String, "not string type is %s", name());
        return _helper.value.string;
    }

    TjsOctet *TjsValue::toOctet() const {
        CLL_ASSERT(type() == TjsValueType::Octet, "not octet type is %s", name());
        return _helper.value.octet;
    }

    TjsObject *TjsValue::toObject() const {
        CLL_ASSERT(type() == TjsValueType::Object, "not object type is %s", name());
        return _helper.value.object;
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

    std::partial_ordering TjsValue::operator<=>(const TjsValue &tjsValue) const {
        if(type() == TjsValueType::Integer && tjsValue.type() == TjsValueType::Integer) {
            return toInteger() <=> tjsValue.toInteger();
        }
        if(type() != TjsValueType::String || tjsValue.type() != TjsValueType::String) {
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
