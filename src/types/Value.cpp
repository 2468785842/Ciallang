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

#include "Octet.hpp"
#include "String.hpp"
#include "TypeConverter.hpp"

namespace cial {

    Value::Value(const Integer value) : _integer(value), _type(ValueType::Integer) {}

    Value::Value(const Real value) : _real(value), _type(ValueType::Real) {}

    Value::Value(String value) : _type(ValueType::String) { _string = new RefCountPointer<String>{ std::move(value) }; }

    Value::Value(Octet value) : _type(ValueType::Octet) { _octet = new RefCountPointer<Octet>{ std::move(value) }; }

    Value::Value(Object *value) : _object(value), _type(ValueType::Object) {}

    Value::~Value() {
        switch(_type) {
            case ValueType::String:
                delete _string;
                break;
            case ValueType::Octet:
                delete _octet;
                break;
            default:;
        }
    }

    Value::Value(Value &&v) noexcept : _type(v._type) {
        switch(_type) {
            case ValueType::Integer:
                _integer = v._integer;
                break;
            case ValueType::Real:
                _real = v._real;
                break;
            case ValueType::String:
                _string = v._string;
                v._string = nullptr;
                break;
            case ValueType::Octet:
                _octet = v._octet;
                v._octet = nullptr;
                break;
            case ValueType::Object:
                _object = v._object;
                v._object = nullptr;
                break;
            case ValueType::Void:
                break;
        }
    }

    Value &Value::operator=(Value &&v) noexcept {
        if(this != &v) {
            this->~Value();
            new(this) Value(std::move(v));
        }
        return *this;
    }

    Value::Value(const Value &v) noexcept {
        _type = v._type;

        switch(v._type) {
            case ValueType::String:
                _string = new RefCountPointer{ *v._string };
                break;
            case ValueType::Octet:
                _octet = new RefCountPointer{ *v._octet };
                break;
            case ValueType::Integer:
                _integer = v._integer;
                break;
            case ValueType::Real:
                _real = v._real;
                break;
            case ValueType::Object:
                _object = v._object;
                break;
            case ValueType::Void:
                break;
        }
    }

    Value &Value::operator=(const Value &value) noexcept {
        if(this != &value) {
            this->~Value();
            new(this) Value(value);
        }
        return *this;
    }

    Ret<Integer> Value::asInteger() const noexcept {
        switch(_type) {
            case ValueType::Void:
                return Ret<Integer>::ok(0);
            case ValueType::Integer:
                return Ret<Integer>::ok(_integer);
            case ValueType::Real:
                return Ret<Integer>::ok(static_cast<Integer>(_real.value()));
            case ValueType::String:
                // TODO:
                // return String->ToInteger();
            case ValueType::Object:
            case ValueType::Octet:
                break;
        }
        return Ret<Integer>::err(ErrCode::InvalidCast, "Invalid type cast to Integer"_str);
    }

    Ret<Real> Value::asReal() const noexcept {
        switch(_type) {
            case ValueType::Void:
                return Ret<Real>::ok(Real{ 0.0 });
            case ValueType::Integer:
                return Ret<Real>::ok(Real{ static_cast<double>(_integer) });
            case ValueType::Real:
                return Ret<Real>::ok(_real);
            case ValueType::String:
                // TODO:
                // return String->ToReal();
            case ValueType::Object:
            case ValueType::Octet:
                break;
        }
        return Ret<Real>::err(ErrCode::InvalidCast, "Invalid type cast to Real"_str);
    }

    Ret<String *> Value::asString() const noexcept {
        if(_type == ValueType::String) {
            return Ret<String *>::ok(_string->getValue());
        }
        return Ret<String *>::err(ErrCode::TypeError, "Invalid type get as String"_str);
    }

    Ret<Octet *> Value::asOctet() const noexcept {
        if(_type == ValueType::Octet) {
            return Ret<Octet *>::ok(_octet->getValue());
        }
        return Ret<Octet *>::err(ErrCode::TypeError, "Invalid type get as Octet"_str);
    }

    Ret<Object *> Value::asObject() const noexcept {
        if(_type == ValueType::Object) {
            return Ret<Object *>::ok(_object);
        }
        return Ret<Object *>::err(ErrCode::TypeError, "Invalid type get as Object"_str);
    }

    bool Value::asBool() const noexcept {
        switch(this->_type) {
            case ValueType::Void:
                return false;
            case ValueType::Object:
                return _object != nullptr;
            case ValueType::String:
                return asInteger().value() != 0;
            case ValueType::Octet:
                return _octet != nullptr;
            case ValueType::Integer:
                return _integer != 0;
            case ValueType::Real:
                return _real.value() != 0;
        }
        return false;
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

    Ret<void> Value::toInteger() noexcept {
        const auto r = this->asInteger();
        if(r.isFailed())
            return Ret<void>::err(r.getErr());

        this->~Value();
        _integer = r.value();
        _type = ValueType::Integer;
        return Ret<void>::ok();
    }

    Ret<void> Value::toReal() noexcept {
        const auto r = this->asReal();
        if(r.isFailed())
            return Ret<void>::err(r.getErr());

        this->~Value();
        _real = r.value();
        _type = ValueType::Real;
        return Ret<void>::ok();
    }

    Ret<void> Value::toString() noexcept {

        switch(_type) {
            case ValueType::Object: {
                this->~Value();
                new(this) Value{ TypeConverter::objectToString(*_object) };
                return Ret<void>::ok();
            }

            case ValueType::String:
                return Ret<void>::ok();

            case ValueType::Integer: {
                this->~Value();
                new(this) Value{ TypeConverter::integerToString(_integer) };
                return Ret<void>::ok();
            }

            case ValueType::Real: {
                this->~Value();
                new(this) Value{ TypeConverter::realToString(_real) };
                return Ret<void>::ok();
            }

            case ValueType::Void:
            case ValueType::Octet:
                break;
        }

        return Ret<void>::err(ErrCode::InvalidCast, "Invalid type cast to String"_str);
    }

    Ret<void> Value::toOctet() const noexcept {
        if(_type == ValueType::Octet)
            return Ret<void>::ok();
        return Ret<void>::err(ErrCode::InvalidCast, "Invalid type cast to Octet"_str);
    }

    Ret<void> Value::toObject() const noexcept {
        if(_type == ValueType::Object)
            return Ret<void>::ok();
        return Ret<void>::err(ErrCode::InvalidCast, "Invalid type cast to Object"_str);
    }

    void Value::toLogicalNot() noexcept {
        const auto r = asBool();
        this->~Value();
        _integer = !r;
        _type = ValueType::Integer;
    }

    Ret<void> Value::toSignChange() noexcept {
        if(!this->isReal()) {
            if(const auto r = asInteger(); !r.isFailed()) {
                this->~Value();
                _integer = -r.value();
                _type = ValueType::Integer;
                return Ret<void>::ok();
            }
        }
        const auto r = asReal();
        if(r.isFailed())
            return Ret<void>::err(r.getErr());
        this->~Value();
        _real = Real{ -r.value().value() };
        _type = ValueType::Real;
        return Ret<void>::ok();
    }

    Ret<Value> Value::add(const Value &value) const noexcept {
        const Err err{ ErrCode::TypeError, "unsupported operand types for `+`"_str };

        if(this->isString() || value.isString()) {
            Value lhs = *this;
            if(auto lhsR = lhs.toString(); lhsR.isFailed())
                return Ret<Value>::err(err);

            Value rhs = value;
            if(auto rhsR = rhs.toString(); rhsR.isFailed())
                return Ret<Value>::err(err);

            auto s1 = lhs.asString();
            if(s1.isFailed())
                return Ret<Value>::err(err);

            auto s2 = rhs.asString();
            if(s2.isFailed())
                return Ret<Value>::err(err);

            String result{ *s1.value() };
            result.append(*s2.value());

            return Ret<Value>::ok(Value{ std::move(result) });
        }

        if(this->_type == value._type) {
            if(this->_type == ValueType::Octet) {
                Octet oct{ *this->_octet->getValue(), *value._octet->getValue() };
                return Ret<Value>::ok(Value{ std::move(oct) });
            }

            if(this->isInteger()) {

                const auto lhs = this->asInteger();
                if(lhs.isFailed())
                    return Ret<Value>::err(err);

                const auto rhs = value.asInteger();
                if(rhs.isFailed())
                    return Ret<Value>::err(err);

                return Ret<Value>::ok(Value{ lhs.value() + rhs.value() });
            }
        }

        if(this->isVoid()) {
            if(value.isInteger() || value.isReal()) {
                return Ret<Value>::ok(value);
            }
        }

        if(value.isVoid()) {
            if(this->isInteger() || this->isReal()) {
                return Ret<Value>::ok(*this);
            }
        }

        const auto lhs = this->asReal();
        if(lhs.isFailed())
            return Ret<Value>::err(err);

        const auto rhs = value.asReal();
        if(rhs.isFailed())
            return Ret<Value>::err(err);

        return Ret<Value>::ok(Value{ lhs.value() + rhs.value() });
    }

    Ret<Value> Value::sub(const Value &value) const noexcept {
        const Err err{ ErrCode::TypeError, "unsupported operand types for `-`"_str };
        if(!this->isReal() && !value.isReal()) {
            const auto lhs = this->asInteger();
            const auto rhs = value.asInteger();
            if(!lhs.isFailed() && !rhs.isFailed()) {
                return Ret<Value>::ok(Value{ lhs.value() - rhs.value() });
            }
        }

        const auto lhs = this->asReal();
        if(lhs.isFailed())
            return Ret<Value>::err(err);
        const auto rhs = value.asReal();
        if(rhs.isFailed())
            return Ret<Value>::err(err);
        return Ret<Value>::ok(Value{ lhs.value() - rhs.value() });
    }

    Ret<Value> Value::mul(const Value &value) const noexcept {
        const Err err{ ErrCode::TypeError, "unsupported operand types for `*`"_str };
        if(!this->isReal() && !value.isReal()) {
            const auto lhs = this->asInteger();
            const auto rhs = value.asInteger();
            if(!lhs.isFailed() && !rhs.isFailed()) {
                return Ret<Value>::ok(Value{ lhs.value() * rhs.value() });
            }
        }

        const auto lhs = this->asReal();
        if(lhs.isFailed())
            return Ret<Value>::err(err);
        const auto rhs = value.asReal();
        if(rhs.isFailed())
            return Ret<Value>::err(err);
        return Ret<Value>::ok(Value{ lhs.value() * rhs.value() });
    }

    Ret<Value> Value::div(const Value &value) const noexcept {
        const Err err{ ErrCode::TypeError, "unsupported operand types for `/`"_str };
        const auto lhs = this->asReal();
        if(lhs.isFailed())
            return Ret<Value>::err(err);
        const auto rhs = value.asReal();
        if(rhs.isFailed())
            return Ret<Value>::err(err);
        return Ret<Value>::ok(Value{ lhs.value() / rhs.value() });
    }

    bool Value::equals(const Value &value) const noexcept {
        const ValueType t1 = this->type();
        const ValueType t2 = value.type();

        // 如果类型相同，进行直接比较
        if(t1 == t2) {
            switch(t1) {
                case ValueType::Integer: {
                    auto v1 = this->asInteger();
                    auto v2 = value.asInteger();
                    return !v1.isFailed() && !v2.isFailed() && v1.value() == v2.value();
                }
                case ValueType::Real: {
                    auto v1 = this->asReal();
                    auto v2 = value.asReal();
                    if(v1.isFailed() || v2.isFailed())
                        return false;
                    Real r1 = v1.value();
                    Real r2 = v2.value();
                    if(r1.isNan() || r2.isNan())
                        return false;
                    if(r1.isInfinity() && r2.isInfinity())
                        return r1.sign() == r2.sign();
                    return r1 == r2;
                }
                case ValueType::String: {
                    auto s1 = this->asString();
                    auto s2 = value.asString();
                    if(s1.isFailed() || s2.isFailed())
                        return false;
                    if(s1.value() == s2.value())
                        return true; // 相同指针
                    if(!s1.value() || !s2.value())
                        return false;
                    return *s1.value() == *s2.value();
                }
                case ValueType::Octet: {
                    auto o1 = this->asOctet();
                    auto o2 = value.asOctet();
                    if(o1.isFailed() || o2.isFailed())
                        return false;
                    if(o1.value() == o2.value())
                        return true; // 相同指针
                    if(!o1.value() || !o2.value())
                        return false;
                    if(o1.value()->getSize() != o2.value()->getSize())
                        return false;
                    return std::memcmp(o1.value()->getData(), o2.value()->getData(), o1.value()->getSize()) == 0;
                }
                case ValueType::Object: {
                    auto obj1 = this->asObject();
                    auto obj2 = value.asObject();
                    if(obj1.isFailed() || obj2.isFailed())
                        return false;
                    return obj1.value() == obj2.value(); // 指针比较，或根据需要深比较
                }
                case ValueType::Void:
                    return true;
                default:
                    return false;
            }
        }

        // 如果任一为String，尝试转换为String比较
        if(t1 == ValueType::String || t2 == ValueType::String) {
            Value lhs = *this;
            if(lhs.toString().isFailed())
                return false;
            Value rhsCopy = value;
            if(rhsCopy.toString().isFailed())
                return false;

            auto s1 = lhs.asString();
            auto s2 = rhsCopy.asString();
            if(s1.isFailed() || s2.isFailed())
                return false;
            if(!s1.value() && !s2.value())
                return true;
            if(!s1.value() || !s2.value())
                return false;
            return *s1.value() == *s2.value();
        }

        // 处理Void与数值/字符串的比较（相当于检查是否为0或空）
        if(t1 == ValueType::Void) {
            switch(t2) {
                case ValueType::Integer: {
                    auto v = value.asInteger();
                    return !v.isFailed() && v.value() == 0;
                }
                case ValueType::Real: {
                    auto v = value.asReal();
                    return !v.isFailed() && v.value().value() == 0.0;
                }
                default:
                    return false;
            }
        }

        if(t2 == ValueType::Void) {
            switch(t1) {
                case ValueType::Integer: {
                    auto v = this->asInteger();
                    return !v.isFailed() && v.value() == 0;
                }
                case ValueType::Real: {
                    auto v = this->asReal();
                    return !v.isFailed() && v.value().value() == 0.0;
                }
                default:
                    return false;
            }
        }

        // 否则，尝试转换为Real进行比较
        auto r1 = this->asReal();
        auto r2 = value.asReal();
        if(r1.isFailed() || r2.isFailed())
            return false;

        Real val1 = r1.value();
        Real val2 = r2.value();

        if(val1.isNan() || val2.isNan())
            return false;
        if(val1.isInfinity() && val2.isInfinity())
            return val1.sign() == val2.sign();
        return val1 == val2;
    }

    bool Value::discernEquals(const Value &value) const noexcept {
        if(this->_type == value._type) {
            switch(this->_type) {
                case ValueType::Object:
                    return _object == value._object;
                case ValueType::String:
                case ValueType::Octet:
                    return equals(value);
                case ValueType::Void:
                    return true;
                case ValueType::Real: {
                    const Real r1 = _real;
                    const Real r2 = value._real;

                    if(r1.isNan() || r2.isNan())
                        return false;
                    if(r1.isInfinity() && r2.isInfinity())
                        return r1.sign() == r2.sign();
                    return r1 == r2;
                }
                case ValueType::Integer:
                    return _integer == value._integer;
            }
            return false;
        }
        return false;
    }

    bool Value::logicalAnd(const Value &value) const noexcept { return this->asBool() && value.asBool(); }

    bool Value::logicalOr(const Value &value) const noexcept { return this->asBool() || value.asBool(); }

    Ret<bool> Value::littlerThan(const Value &value) const noexcept {
        const Err err{ ErrCode::TypeError, "unsupported operand types for `<`"_str };

        if(!this->isString() || !value.isString()) {
            if(this->isInteger() && value.isInteger()) {
                return Ret<bool>::ok(this->_integer < value._integer);
            }
            const auto lhs = this->asReal();
            const auto rhs = value.asReal();
            if(lhs.isFailed() || rhs.isFailed())
                return Ret<bool>::err(err);
            return Ret<bool>::ok(lhs.value() < rhs.value());
        }
        const auto lhs = this->asString();
        const auto rhs = value.asString();
        if(lhs.isFailed() || rhs.isFailed())
            return Ret<bool>::err(err);

        return Ret<bool>::ok(std::strcmp(lhs.value()->getData(), rhs.value()->getData()) < 0);
    }

    Ret<bool> Value::greaterThan(const Value &value) const noexcept {
        const Err err{ ErrCode::TypeError, "unsupported operand types for `>`"_str };

        if(!this->isString() || !value.isString()) {
            if(this->isInteger() && value.isInteger()) {
                return Ret<bool>::ok(this->_integer > value._integer);
            }
            const auto lhs = this->asReal();
            const auto rhs = value.asReal();
            if(lhs.isFailed() || rhs.isFailed())
                return Ret<bool>::err(err);
            return Ret<bool>::ok(lhs.value() > rhs.value());
        }
        const auto lhs = this->asString();
        const auto rhs = value.asString();
        if(lhs.isFailed() || rhs.isFailed())
            return Ret<bool>::err(err);

        return Ret<bool>::ok(std::strcmp(lhs.value()->getData(), rhs.value()->getData()) > 0);
    }

    std::ostream &operator<<(std::ostream &os, const Value &d) {
        switch(d.type()) {
            case ValueType::Integer: {
                const auto r = d.asInteger();
                if(r.isFailed())
                    throw std::logic_error("Invalid type cast to Integer");
                return os << r.value();
            }
            case ValueType::Real: {
                const auto r = d.asReal();
                if(r.isFailed())
                    throw std::logic_error("Invalid type cast to Real");
                return os << r.value();
            }
            case ValueType::String: {
                const auto r = d.asString();
                if(r.isFailed())
                    throw std::logic_error("Invalid type cast to String");
                return os << "\"" << *r.value() << "\"";
            }
            case ValueType::Octet:
                return os << "<octet>";
            case ValueType::Object:
                return os << "<object>";
            case ValueType::Void:
                return os << "void";
        }
        return os << "unknown";
    }

} // namespace cial
