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

#include <cstddef>
#include <fmt/ostream.h>

#include "Types.hpp"

#include "config.h"

#include "Octet.hpp"
#include "Real.hpp"
#include "String.hpp"

#include "common/Ret.hpp"
#include "gc/gc.hpp"

namespace cial {

    /**
     * 字符串/字节串的共享负载块：引用计数 + 堆上 T。
     * Value 持有 SharedPayload*，拷贝只递增计数（零堆分配、单层间接）；
     * 取代旧的 Value→堆RefCountPointer→堆RefCountBox→堆String 三层布局。
     */
    template <typename T>
    struct SharedPayload {
        u32 ref{ 1 };
        u8 embedded{ 0 }; // 1 = data 与 payload 同块（makeSingle），析构只 operator delete 本块
        T *data;

        explicit SharedPayload(T *d) : data(d) {}

        SharedPayload(const SharedPayload &) = delete;
        SharedPayload &operator=(const SharedPayload &) = delete;

        static SharedPayload *make(T *d) { return new SharedPayload{ d }; }

        // 单块分配：payload 头 + T 紧随其后就地构造。String/Octet 构造是拼接
        // 热路径（原 make(new T) 每次两次堆分配），string_concat 差距主因之一
        static SharedPayload *makeSingle(T &&v) {
            auto *p = static_cast<SharedPayload *>(::operator new(sizeof(SharedPayload) + sizeof(T)));
            p->ref = 1;
            p->embedded = 1;
            p->data = reinterpret_cast<T *>(p + 1);
            new(p->data) T(std::move(v));
            return p;
        }

        static void retain(SharedPayload *p) noexcept { ++p->ref; }

        static void release(SharedPayload *p) noexcept {
            if(--p->ref == 0) {
                p->data->~T();
                if(p->embedded) {
                    ::operator delete(p);
                } else {
                    delete p->data;
                    delete p;
                }
            }
        }
    };

    class Value {

    public:
        Value() noexcept = default;

        // 构造/析构/拷贝移动在类内定义：整数与对象路径现场消解为几次访存，
        // 跨 TU 外联调用曾在原生调用画像中占 Value 生命周期总样本的 ~40%
        // （2026-10-31 native_stress 采样，见 doc/performance.md）
        explicit Value(const Integer value) {
            _data.integer = value;
            type(ValueType::Integer);
        }

        explicit Value(const Real value) {
            new(&_data.real) Real(value);
            type(ValueType::Real);
        }

        explicit Value(String value);

        explicit Value(Octet value);

        explicit Value(Object *value) {
            _data.object = value;
            type(ValueType::Object);
        }

        CIAL_INLINE ~Value() {
            if(isInlineString())
                _smallString.~String();
            else if(isString())
                SharedPayload<String>::release(_data.string);
            else if(isOctet())
                SharedPayload<Octet>::release(_data.octet);
        }

        CIAL_INLINE Value(Value &&v) noexcept {
            const auto kind = v.type();
            if(v.isInlineString()) {
                new(&_smallString) String(std::move(v._smallString));
                v._smallString.~String();
                tag() |= inlineStringTag << 5;
            } else {
                switch(kind) {
                    case ValueType::Integer:
                        _data.integer = v._data.integer;
                        break;
                    case ValueType::Real:
                        new(&_data.real) Real(v._data.real);
                        break;
                    case ValueType::String:
                        _data.string = v._data.string;
                        break;
                    case ValueType::Octet:
                        _data.octet = v._data.octet;
                        break;
                    case ValueType::Object:
                        _data.object = v._data.object;
                        break;
                    case ValueType::Void:
                        _data.integer = 0;
                        break;
                }
                type(kind);
            }
            v.type(ValueType::Void);
        }

        CIAL_INLINE Value &operator=(Value &&v) noexcept {
            if(this != &v) {
                this->~Value();
                new(this) Value(std::move(v));
            }
            return *this;
        }

        CIAL_INLINE Value(const Value &v) noexcept {
            const auto kind = v.type();
            if(v.isInlineString()) {
                new(&_smallString) String(v._smallString);
                tag() |= inlineStringTag << 5;
            } else {
                switch(kind) {
                    case ValueType::Integer:
                        _data.integer = v._data.integer;
                        break;
                    case ValueType::Real:
                        new(&_data.real) Real(v._data.real);
                        break;
                    case ValueType::String:
                        _data.string = v._data.string;
                        SharedPayload<String>::retain(_data.string);
                        break;
                    case ValueType::Octet:
                        _data.octet = v._data.octet;
                        SharedPayload<Octet>::retain(_data.octet);
                        break;
                    case ValueType::Object:
                        _data.object = v._data.object;
                        break;
                    case ValueType::Void:
                        _data.integer = 0;
                        break;
                }
                type(kind);
            }
        }

        CIAL_INLINE Value &operator=(const Value &value) noexcept {
            if(this != &value) {
                this->~Value();
                new(this) Value(value);
            }
            return *this;
        }

        [[nodiscard]] ValueType type() const noexcept {
            const u8 raw = tag() >> 5;
            return raw == inlineStringTag ? ValueType::String : static_cast<ValueType>(raw);
        }

        void type(const ValueType type) noexcept { tag() = static_cast<u8>(type) << 5; }

        [[nodiscard]] bool isVoid() const noexcept { return tag() == 0; }

        [[nodiscard]] bool isInteger() const noexcept { return tag() == (static_cast<u8>(ValueType::Integer) << 5); }

        [[nodiscard]] bool isReal() const noexcept { return tag() == (static_cast<u8>(ValueType::Real) << 5); }

        [[nodiscard]] bool isString() const noexcept { return (tag() & 0x60) == 0x40; }
        [[nodiscard]] bool isInlineString() const noexcept { return (tag() >> 5) == inlineStringTag; }

        [[nodiscard]] bool isOctet() const noexcept { return tag() == (static_cast<u8>(ValueType::Octet) << 5); }

        [[nodiscard]] bool isObject() const noexcept { return tag() == (static_cast<u8>(ValueType::Object) << 5); }

        [[nodiscard]] const char *name() const;

        // 数值取整：Integer/Void/Real 快路径类内内联（派发循环 8 处调用点 +
        // 算术族内部各两次），String 解析/Octet/Object 报错走冷慢路径；
        // 拆分动机同上——外联调用曾在 loop_int 画像中占 ~4.4%
        [[nodiscard]] Ret<Integer> asInteger() const noexcept {
            switch(type()) {
                case ValueType::Void:
                    return Ret<Integer>::ok(0);
                case ValueType::Integer:
                    return Ret<Integer>::ok(_data.integer);
                case ValueType::Real:
                    return Ret<Integer>::ok(static_cast<Integer>(_data.real.value()));
                default:
                    return asIntegerSlow();
            }
        }
        [[nodiscard]] Ret<Integer> asIntegerSlow() const noexcept;

        // 热路径直取:调用方保证 isInteger() 为真(如 dispatch 整数特化),
        // 绕过 Ret<T> optional 装配
        [[nodiscard]] Integer asIntegerUnsafe() const noexcept { return _data.integer; }
        [[nodiscard]] bool isIntegerFast() const noexcept {
            return tag() == (static_cast<u8>(ValueType::Integer) << 5);
        }
        // 整数直写(调用方负责先把 type 置 Integer,避免覆盖堆持有类型的联合体)
        void asIntegerStore(const Integer v) noexcept { _data.integer = v; }

        // octet 负载直装(Constant 常量缓存路径;调用方负责 retain 与置 type)
        void octetStore(SharedPayload<Octet> *p) noexcept { _data.octet = p; }

        [[nodiscard]] Ret<Real> asReal() const noexcept;

        // 借用所属 Value 的字符串；短串直接内联，不能借用临时 Value 后跨全表达式使用。
        [[nodiscard]] Ret<String *> asString() const noexcept;

        [[nodiscard]] Ret<Octet *> asOctet() const noexcept;

        [[nodiscard]] CIAL_INLINE Ret<Object *> asObject() const noexcept {
            if(type() == ValueType::Object) {
                return Ret<Object *>::ok(_data.object);
            }
            return Ret<Object *>::err(ErrCode::TypeError, "Invalid type get as Object"_str);
        }

        [[nodiscard]] bool asBool() const noexcept;

        [[nodiscard]] Ret<void> toInteger() noexcept;

        [[nodiscard]] Ret<void> toReal() noexcept;

        [[nodiscard]] Ret<void> toString() noexcept;

        [[nodiscard]] Ret<void> toOctet() const noexcept;

        [[nodiscard]] Ret<void> toObject() const noexcept;

        void toLogicalNot() noexcept;

        // 按位非（旧版 VM_BNOT/bitnot）：~asInteger（real 截断、字符串解析，
        // 与 TJS AsInteger 链一致）；原地
        Ret<void> bitNot() noexcept;

        // 数值规范化（旧版 VM_NUM/tonumber）：int/real 不变、字符串解析为数值
        // （整样貌→int 否则 real）、void → 0、其余报错；原地
        Ret<void> toNumber() noexcept;

        [[nodiscard]] Ret<void> toSignChange() noexcept;

        [[nodiscard]] Ret<Value> add(const Value &value) const noexcept;

        [[nodiscard]] Ret<Value> sub(const Value &value) const noexcept;
        [[nodiscard]] Ret<Value> mul(const Value &value) const noexcept;
        [[nodiscard]] Ret<Value> div(const Value &value) const noexcept;
        [[nodiscard]] Ret<Value> idiv(const Value &value) const noexcept;
        // 取模：双 Integer 快路径类内内联（含除零守卫），其余类型组合走
        // modSlow（asInteger 语义含 Real 截断与字符串解析，不能在快路径冒进）
        [[nodiscard]] Ret<Value> mod(const Value &value) const noexcept {
            if(isIntegerFast() && value.isIntegerFast()) {
                if(value._data.integer == 0)
                    return Ret<Value>::err(ErrCode::DivideByZero, "Divide by zero"_str);
                return Ret<Value>::ok(Value{ _data.integer % value._data.integer });
            }
            return modSlow(value);
        }
        [[nodiscard]] Ret<Value> modSlow(const Value &value) const noexcept;

        [[nodiscard]] Ret<Value> bitwiseXor(const Value &value) const noexcept;
        [[nodiscard]] Ret<Value> bitwiseAnd(const Value &value) const noexcept;
        [[nodiscard]] Ret<Value> bitwiseOr(const Value &value) const noexcept;

        [[nodiscard]] Ret<Value> bitwiseLeftShift(const Value &value) const noexcept;
        [[nodiscard]] Ret<Value> bitwiseRightShift(const Value &value) const noexcept;
        [[nodiscard]] Ret<Value> bitwiseUnsignedRightShift(const Value &value) const noexcept;

        [[nodiscard]] bool equals(const Value &value) const noexcept;

        [[nodiscard]] bool discernEquals(const Value &value) const noexcept;

        [[nodiscard]] bool logicalAnd(const Value &value) const noexcept;

        [[nodiscard]] bool logicalOr(const Value &value) const noexcept;

        [[nodiscard]] Ret<bool> littlerThan(const Value &value) const noexcept;

        [[nodiscard]] Ret<bool> greaterThan(const Value &value) const noexcept;

    private:
        friend struct Constant; // 常量字符串负载缓存直接装配 Value

        struct Data {
            union {
                Integer integer{};
                Real real;
                SharedPayload<String> *string;
                SharedPayload<Octet> *octet;
                Object *object;
            };
            u8 reserved[7]; // 仅供布局填充，不参与语义读取或整块比较；无须在热路径清零。
            u8 control;
            Data() noexcept : integer(0), control(0) {}
        };
        static_assert(sizeof(Data) == 16 && offsetof(Data, control) == 15);
        union {
            Data _data{};
            String _smallString;
        };

        // String 保留的末字节高 3 位承载 Value 标签；内部标签 6 表示
        // 内联 String，外部仍报告 ValueType::String。所有 Value 维持 16B。
        static constexpr u8 inlineStringTag = 6;
        u8 &tag() noexcept { return reinterpret_cast<u8 *>(this)[15]; }
        u8 tag() const noexcept { return reinterpret_cast<const u8 *>(this)[15]; }

        friend std::ostream &operator<<(std::ostream &os, const Value &d);
    };

    static_assert(sizeof(Value) == 16, "Value must fit two machine words");
} // namespace cial

// support fmt::format
template <>
struct fmt::formatter<cial::Value> : ostream_formatter {};
