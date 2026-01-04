//
// Created by LiDong on 2026/1/4.
//
#pragma once

#include <functional>

#include "runtime/Runtime.hpp"
#include "types/Function.hpp"

namespace Cial {

#define TYPE_ID_PAIR_ENUMS(X)                                                                                          \
    X(Integer, "Integer")                                                                                              \
    X(Real, "Real")                                                                                                    \
    X(Object, "Object")                                                                                                \
    X(Octet, "Octet")                                                                                                  \
    X(String, "String")                                                                                                \
    X(Array, "Array")                                                                                                  \
    X(Dictionary, "Dictionary")                                                                                        \
    X(Exception, "Exception")                                                                                          \
    X(Date, "Date")                                                                                                    \
    X(Math, "Math")                                                                                                    \
    X(RegExp, "RegExp")

    enum class TypeId {
#define TYPE_ID_ENUM_CLASS(O, N) O,
        None,
        TYPE_ID_PAIR_ENUMS(TYPE_ID_ENUM_CLASS)
#undef TYPE_ID_ENUM_CLASS
    };

    template <typename T>
    struct NativeTypeToTypeId {
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
            }
        }();
    };

    struct ValueToTypeId {
        static TypeId getId(const Value &v) {
            if(v.isInteger())
                return NativeTypeToTypeId<Integer>::id;
            if(v.isReal())
                return NativeTypeToTypeId<Real>::id;
            if(v.isObject())
                return NativeTypeToTypeId<Object>::id;
            if(v.isOctet())
                return NativeTypeToTypeId<Octet>::id;
            if(v.isString())
                return NativeTypeToTypeId<String>::id;
            return TypeId::None;
        }
    };

    struct MethodTable {
        Map<Atom, NativeFunction *> staticMethods; // 静态方法，如 Array.from()
        Map<Atom, NativeFunction *> instanceMethods; // 实例方法，如 arr.push()
    };

    class NativeRegister {
    public:
        explicit NativeRegister(Runtime &rt) : _rt(rt) {}

        template <typename T, typename Callable>
        void registerMethod(const Atom name, Callable &&fn, const bool isStatic) {
            const TypeId type = NativeTypeToTypeId<T>::id;
            auto &[staticMethods, instanceMethods] = tables[type];
            auto *nativeFn = _rt.create<NativeFunction>(std::forward<Callable>(fn));
            if(isStatic) {
                staticMethods[name] = nativeFn;
            } else {
                instanceMethods[name] = nativeFn;
            }
        }

        template <typename T, typename Callable>
        void registerMethod(const String &name, Callable &&fn, const bool isStatic) {
            const Atom a = _rt.atomTable.intern(name);
            return registerMethod<T>(a, std::forward<Callable>(fn), isStatic);
        }

        template <typename T>
        [[nodiscard]] NativeFunction *findMethod(const Atom name, const bool isStatic) const {
            const TypeId type = NativeTypeToTypeId<T>::id;
            const auto it = tables.find(type);
            if(it == tables.end())
                return nullptr;
            const auto &methods = isStatic ? it->second.staticMethods : it->second.instanceMethods;
            const auto fnIt = methods.find(name);
            return fnIt != methods.end() ? fnIt->second : nullptr;
        }

        template <typename T>
        [[nodiscard]] NativeFunction *findMethod(const String &name, const bool isStatic) const {
            const Atom a = _rt.atomTable.intern(name);
            return findMethod<T>(a, isStatic);
        }

    private:
        Map<TypeId, MethodTable> tables{};
        Runtime &_rt;

        friend class Context;
    };
} // namespace Cial