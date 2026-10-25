//
// Created by LiDong on 2026/1/4.
//
#pragma once

#include "runtime/Runtime.hpp"
#include "types/Array.hpp"
#include "types/Dictionary.hpp"
#include "types/Function.hpp"
#include "types/RegExp.hpp"

namespace cial {

    enum class TypeId {
        None,
        Integer,
        Real,
        Object,
        Octet,
        String,
        Array,
        Dictionary,
        RegExp,
    };

    template <typename T>
    struct TypeTraits {
        static constexpr TypeId id = [] {
            if constexpr(std::is_same_v<T, Integer>) {
                return TypeId::Integer;
            } else if constexpr(std::is_same_v<T, Real>) {
                return TypeId::Real;
            } else if constexpr(std::is_same_v<T, Object>) {
                return TypeId::Object;
            } else if constexpr(std::is_same_v<T, Octet>) {
                return TypeId::Octet;
            } else if constexpr(std::is_same_v<T, String>) {
                return TypeId::String;
            } else if constexpr(std::is_same_v<T, Array>) {
                return TypeId::Array;
            } else if constexpr(std::is_same_v<T, Dictionary>) {
                return TypeId::Dictionary;
            } else if constexpr(std::is_same_v<T, RegExp>) {
                return TypeId::RegExp;
            } else {
                static_assert(!std::is_same_v<T, T> && "type is not support");
                return TypeId::None;
            }
        }();
    };

    struct ValueToTypeId {
        static TypeId getId(const Value &v) {
            if(v.isInteger())
                return TypeTraits<Integer>::id;
            if(v.isReal())
                return TypeTraits<Real>::id;
            if(v.isOctet())
                return TypeTraits<Octet>::id;
            if(v.isString())
                return TypeTraits<String>::id;
            if(v.isObject()) {
                // 容器对象（Array/Dictionary/RegExp 的原生方法表分派）
                const auto *obj = v.asObject().unwrap();
                if(dynamic_cast<const Array *>(obj) != nullptr)
                    return TypeId::Array;
                if(dynamic_cast<const Dictionary *>(obj) != nullptr)
                    return TypeId::Dictionary;
                if(dynamic_cast<const RegExp *>(obj) != nullptr)
                    return TypeId::RegExp;
                return TypeId::Object;
            }
            return TypeId::None;
        }
    };

    // 原生只读属性 getter：thisObj 为宿主值（如 String），返回属性值（如 str.length）
    using NativePropGetter = Value (*)(const Value &thisObj);

    struct MethodTable {
        Map<Atom, NativeFunction *> instanceMethods; // instance method，like `arr.push()`
        Map<Atom, NativePropGetter> instanceProperties; // 只读属性，like `str.length`
    };

    class NativeRegister {
    public:
        explicit NativeRegister(Runtime &rt) : _rt(rt) {}

        template <typename Callable>
        void registerMethod(const TypeId typeId, const Atom name, Callable &&fn) {
            auto *nativeFn = _rt.create<NativeFunction>(std::forward<Callable>(fn)).get();
            tables[typeId].instanceMethods[name] = nativeFn;
        }

        template <typename T, typename Callable>
        void registerMethod(const Atom name, Callable &&fn) {
            const TypeId type = TypeTraits<T>::id;
            registerMethod(type, name, std::forward<Callable>(fn));
        }

        template <typename T, typename Callable>
        void registerMethod(const String &name, Callable &&fn) {
            const Atom a = _rt.atomTable.intern(name);
            return registerMethod<T>(a, std::forward<Callable>(fn));
        }

        void registerProperty(const TypeId typeId, const Atom name, const NativePropGetter getter) {
            tables[typeId].instanceProperties[name] = getter;
        }

        template <typename T>
        void registerProperty(const String &name, const NativePropGetter getter) {
            registerProperty(TypeTraits<T>::id, _rt.atomTable.intern(name), getter);
        }

        [[nodiscard]] NativeFunction *findMethod(const TypeId typeId, const Atom name) const {
            const auto it = tables.find(typeId);
            if(it == tables.end())
                return nullptr;
            const auto &methods = it->second.instanceMethods;
            const auto fnIt = methods.find(name);
            return fnIt != methods.end() ? fnIt->second : nullptr;
        }

        [[nodiscard]] NativePropGetter findProperty(const TypeId typeId, const Atom name) const {
            const auto it = tables.find(typeId);
            if(it == tables.end())
                return nullptr;
            const auto &props = it->second.instanceProperties;
            const auto propIt = props.find(name);
            return propIt != props.end() ? propIt->second : nullptr;
        }

        template <typename T>
        [[nodiscard]] NativeFunction *findMethod(const Atom name) const {
            const TypeId type = TypeTraits<T>::id;
            return findMethod(type, name);
        }

        template <typename T>
        [[nodiscard]] NativeFunction *findMethod(const String &name) const {
            const Atom a = _rt.atomTable.intern(name);
            return findMethod<T>(a);
        }

    private:
        Map<TypeId, MethodTable> tables{};
        Runtime &_rt;

        friend class Context;
    };
} // namespace cial