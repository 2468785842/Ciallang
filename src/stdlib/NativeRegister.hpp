//
// Created by LiDong on 2026/1/4.
//
#pragma once

#include "runtime/Runtime.hpp"
#include "types/Function.hpp"

namespace cial {

    enum class TypeId {
        None,
        Integer,
        Real,
        Object,
        Octet,
        String,
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
            if(v.isObject())
                return TypeTraits<Object>::id;
            if(v.isOctet())
                return TypeTraits<Octet>::id;
            if(v.isString())
                return TypeTraits<String>::id;
            return TypeId::None;
        }
    };

    struct MethodTable {
        Map<Atom, NativeFunction *> instanceMethods; // instance method，like `arr.push()`
    };

    class NativeRegister {
    public:
        explicit NativeRegister(Runtime &rt) : _rt(rt) {}

        template <typename Callable>
        void registerMethod(const TypeId typeId, const Atom name, Callable &&fn) {
            auto &[instanceMethods] = tables[typeId];
            auto *nativeFn = _rt.create<NativeFunction>(std::forward<Callable>(fn)).get();
            instanceMethods[name] = nativeFn;
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

        [[nodiscard]] NativeFunction *findMethod(const TypeId typeId, const Atom name) const {
            const auto it = tables.find(typeId);
            if(it == tables.end())
                return nullptr;
            const auto &[instanceMethods] = it->second;
            const auto fnIt = instanceMethods.find(name);
            return fnIt != instanceMethods.end() ? fnIt->second : nullptr;
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