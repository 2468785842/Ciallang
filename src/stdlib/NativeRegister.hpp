//
// Created by LiDong on 2026/1/4.
//
#pragma once

#include "runtime/Runtime.hpp"
#include "types/Function.hpp"
#include "types/NativeObject.hpp"
#include "types/TypeId.hpp"

namespace cial {

    template <typename T>
    struct TypeTraits {
        static constexpr TypeId id = [] {
            if constexpr(requires { T::nativeTypeId; })
                return T::nativeTypeId;
            else if constexpr(std::is_same_v<T, Integer>)
                return TypeId::Integer;
            else if constexpr(std::is_same_v<T, Real>)
                return TypeId::Real;
            else if constexpr(std::is_same_v<T, String>)
                return TypeId::String;
            else if constexpr(std::is_same_v<T, Octet>)
                return TypeId::Octet;
            else
                return TypeId::None;
        }();
        static NativeTypeKey key() noexcept {
            if constexpr(id != TypeId::None)
                return id;
            else
                return nativeTypeKeyFor<std::remove_cvref_t<T>>();
        }
    };

    struct ValueToNativeTypeKey {
        static NativeTypeKey getId(const Value &v) {
            if(v.isInteger())
                return TypeTraits<Integer>::id;
            if(v.isReal())
                return TypeTraits<Real>::id;
            if(v.isOctet())
                return TypeTraits<Octet>::id;
            if(v.isString())
                return TypeTraits<String>::id;
            if(v.isObject())
                // 对象提供开放的原生类型身份；VM 内部判型仍使用 TypeId。
                return v.asObject().unwrap()->nativeTypeKey();
            return TypeId::None;
        }
    };

    using ValueToTypeId = ValueToNativeTypeKey; // 旧名称兼容

    // 原生只读属性 getter：thisObj 为宿主值（如 String），返回属性值（如 str.length）
    using NativePropGetter = Value (*)(const Value &thisObj);

    struct NativeAccessor {
        NativeFunction *getter{};
        NativeFunction *setter{};
    };

    struct NativeMember {
        NativeFunction *method{};
        NativePropGetter getter{};
        NativeAccessor accessor{};
    };

    struct MethodTable {
        Map<Atom, NativeFunction *> instanceMethods; // instance method，like `arr.push()`
        Map<Atom, NativeAccessor> accessors;
        Map<Atom, NativePropGetter> instanceProperties; // 只读属性，like `str.length`
    };

    class NativeRegister {
    public:
        explicit NativeRegister(Runtime &rt) : _rt(rt) {}

        template <typename Callable>
        void registerMethod(const NativeTypeKey typeId, const Atom name, Callable &&fn) {
            auto *nativeFn = _rt.create<NativeFunction>(std::forward<Callable>(fn)).get();
            tables[typeId].instanceMethods[name] = nativeFn;
            ++revision;
        }

        template <typename T, typename Callable>
        void registerMethod(const Atom name, Callable &&fn) {
            const NativeTypeKey type = TypeTraits<T>::key();
            registerMethod(type, name, std::forward<Callable>(fn));
        }

        template <typename T, typename Callable>
        void registerMethod(const String &name, Callable &&fn) {
            const Atom a = _rt.atomTable.intern(name);
            return registerMethod<T>(a, std::forward<Callable>(fn));
        }

        void registerProperty(const NativeTypeKey typeId, const Atom name, const NativePropGetter getter) {
            tables[typeId].instanceProperties[name] = getter;
            ++revision;
        }

        template <typename T>
        void registerProperty(const String &name, const NativePropGetter getter) {
            registerProperty(TypeTraits<T>::key(), _rt.atomTable.intern(name), getter);
        }

        // 一次类型表定位，同时取得三类成员供 VM 读取及调用点缓存使用。
        [[nodiscard]] NativeMember findMember(const NativeTypeKey type, const Atom name) const {
            const auto table = tables.find(type);
            if(table == tables.end())
                return {};
            const auto &members = table->second;
            const auto method = members.instanceMethods.find(name);
            const auto getter = members.instanceProperties.find(name);
            const auto accessor = members.accessors.find(name);
            return {
                .method = method == members.instanceMethods.end() ? nullptr : method->second,
                .getter = getter == members.instanceProperties.end() ? nullptr : getter->second,
                .accessor = accessor == members.accessors.end() ? NativeAccessor{} : accessor->second,
            };
        }

        [[nodiscard]] NativeFunction *findMethod(const NativeTypeKey typeId, const Atom name) const {
            const auto it = tables.find(typeId);
            if(it == tables.end())
                return nullptr;
            const auto &methods = it->second.instanceMethods;
            const auto fnIt = methods.find(name);
            return fnIt != methods.end() ? fnIt->second : nullptr;
        }

        [[nodiscard]] NativePropGetter findProperty(const NativeTypeKey typeId, const Atom name) const {
            const auto it = tables.find(typeId);
            if(it == tables.end())
                return nullptr;
            const auto &props = it->second.instanceProperties;
            const auto propIt = props.find(name);
            return propIt != props.end() ? propIt->second : nullptr;
        }

        template <typename T>
        [[nodiscard]] NativeFunction *findMethod(const Atom name) const {
            const NativeTypeKey type = TypeTraits<T>::key();
            return findMethod(type, name);
        }

        template <typename T>
        [[nodiscard]] NativeFunction *findMethod(const String &name) const {
            const Atom a = _rt.atomTable.intern(name);
            return findMethod<T>(a);
        }

        void registerAccessor(NativeTypeKey type, Atom name, NativeFunction *getter, NativeFunction *setter) {
            tables[type].accessors[name] = { getter, setter };
            ++revision;
        }

        [[nodiscard]] NativeAccessor findAccessor(NativeTypeKey type, Atom name) const {
            const auto table = tables.find(type);
            if(table == tables.end())
                return {};
            const auto it = table->second.accessors.find(name);
            return it == table->second.accessors.end() ? NativeAccessor{} : it->second;
        }

        Map<NativeTypeKey, std::shared_ptr<NativeTypeBinding>> bindings;

        u64 revision{}; // VM 调用点缓存随宿主注册/替换方法失效

    private:
        Map<NativeTypeKey, MethodTable> tables{};
        Runtime &_rt;

        friend class Context;
    };
} // namespace cial