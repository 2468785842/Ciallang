/*
 * Copyright (c) 2025/12/30 上午8:08
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */
#pragma once

#include <algorithm>
#include <ranges>

#include "parser/PreProcessor.hpp"
#include "runtime/Runtime.hpp"
#include "stdlib/NativeRegister.hpp"
#include "vm/FastRegisterPool.hpp"

namespace cial {
    namespace binding {
        class Module;
    }
    class GlobalObject;
    struct CallFrame;

    class Context : public MarkRootProvider {
    public:
        // Stack Max Depth Is 1024
        static constexpr auto maxCallDepth = 1024;

        explicit Context(Runtime &rt);
        ~Context() noexcept override;
        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;

        // GC 运行时根入口：供 Runtime 在分配触发回收时调用
        void collectMark() const override;

        void registerGlobal(const String &name, const Value &value) const;

        template <typename T, typename... Args>
        T *registerClass(const String &name, Args &&...args) {
            recordApiTypeName(TypeTraits<T>::key(), name.toStdStr());
            auto object = rt().create<T>(std::forward<Args>(args)...);
            registerGlobal(name, Value{ object.get() });
            return object.get();
        }

        template <typename T, typename Callable>
        void registerMethod(const String &name, Callable &&fn) {
            return nativeRegister().registerMethod<T>(name, std::forward<Callable>(fn));
        }

        template <typename Callable>
        void registerMethod(NativeTypeKey type, const String &name, Callable &&fn) {
            nativeRegister().registerMethod(type, rt().atomTable.intern(name), std::forward<Callable>(fn));
        }

        void registerProperty(NativeTypeKey type, const String &name, NativePropGetter getter) {
            nativeRegister().registerProperty(type, rt().atomTable.intern(name), getter);
        }

        template <typename T>
        void registerProperty(const String &name, const NativePropGetter getter) {
            return nativeRegister().registerProperty<T>(name, getter);
        }

        [[nodiscard]] NativeMember findNativeMember(const NativeTypeKey type, const Atom name) const {
            return nativeRegister().findMember(type, name);
        }

        [[nodiscard]] NativeFunction *findMethod(const NativeTypeKey typeId, const Atom a) const {
            return nativeRegister().findMethod(typeId, a);
        }

        [[nodiscard]] NativePropGetter findProperty(const NativeTypeKey typeId, const Atom a) const {
            return nativeRegister().findProperty(typeId, a);
        }

        void registerAccessor(NativeTypeKey type, const String &name, NativeFunction *getter,
                              NativeFunction *setter = nullptr) {
            nativeRegister().registerAccessor(type, rt().atomTable.intern(name), getter, setter);
        }

        [[nodiscard]] NativeAccessor findAccessor(NativeTypeKey type, Atom name) const {
            return nativeRegister().findAccessor(type, name);
        }

        [[nodiscard]] std::shared_ptr<NativeTypeBinding> nativeBinding(NativeTypeKey key) const {
            const auto &bindings = nativeRegister().bindings;
            const auto it = bindings.find(key);
            return it == bindings.end() ? nullptr : it->second;
        }

        void registerNativeBinding(std::shared_ptr<NativeTypeBinding> binding) {
            nativeRegister().bindings.emplace(binding->key, std::move(binding));
        }

        [[nodiscard]] u64 nativeRevision() const { return nativeRegister().revision; }

        // 注册期 API 元数据登记与读取（doc/api-metadata.md）；数据在 NativeRegister，
        // 生命周期同 Context，纯 C++ 所有权。
        void recordApiTypeName(NativeTypeKey key, std::string name) {
            nativeRegister().apiCatalog.typeNames[key] = std::move(name);
        }
        void recordApiMember(NativeTypeKey key, ApiMeta meta) {
            api::normalize(meta);
            auto &members = nativeRegister().apiCatalog.typeMembers[key];
            // 注册表按名字覆盖（MethodTable/属性表同名替换）；目录保持同语义，
            // 重注册回填（如 registerScriptsLib 的二次绑定）不产生重复条目
            const auto existing = std::ranges::find_if(
                members, [&](const ApiMeta &entry) { return entry.name == meta.name && entry.kind == meta.kind; });
            if(existing != members.end())
                *existing = std::move(meta);
            else
                members.push_back(std::move(meta));
        }
        void recordApiGlobal(ApiMeta meta) {
            api::normalize(meta);
            nativeRegister().apiCatalog.globals.push_back(std::move(meta));
        }
        [[nodiscard]] const ApiCatalog &apiCatalog() const { return nativeRegister().apiCatalog; }

        template <typename T>
        class NativeTypeBuilder {
            Context &_ctx;

        public:
            explicit NativeTypeBuilder(Context &ctx) : _ctx(ctx) {}
            template <typename Callable>
            NativeTypeBuilder &method(const String &name, Callable &&fn, std::string doc = {}) {
                _ctx.registerMethod<T>(name, std::forward<Callable>(fn));
                _ctx.recordApiMember(TypeTraits<T>::key(),
                                     api::makeMeta<Callable>(ApiKind::Method, name.toStdStr(), {}, std::move(doc)));
                return *this;
            }
            NativeTypeBuilder &property(const String &name, NativePropGetter getter, std::string doc = {}) {
                _ctx.registerProperty<T>(name, getter);
                // NativePropGetter 返回 Value，具体属性类型注册点未声明时推导不出
                ApiMeta meta{ .kind = ApiKind::Property, .name = name.toStdStr(), .ret = "any", .doc = std::move(doc) };
                _ctx.recordApiMember(TypeTraits<T>::key(), std::move(meta));
                return *this;
            }
            template <typename Callable>
            NativeTypeBuilder &method(const String &name, Callable &&fn, const ApiMeta &description) {
                _ctx.registerMethod<T>(name, std::forward<Callable>(fn));
                _ctx.recordApiMember(
                    TypeTraits<T>::key(),
                    api::annotate(api::makeMeta<Callable>(ApiKind::Method, name.toStdStr()), description));
                return *this;
            }
            NativeTypeBuilder &property(const String &name, NativePropGetter getter, const ApiMeta &description) {
                _ctx.registerProperty<T>(name, getter);
                _ctx.recordApiMember(
                    TypeTraits<T>::key(),
                    api::annotate(ApiMeta{ .kind = ApiKind::Property, .name = name.toStdStr(), .ret = "any" },
                                  description));
                return *this;
            }
        };

        template <typename T>
        NativeTypeBuilder<T> nativeType() {
            return NativeTypeBuilder<T>{ *this };
        }

        binding::Module module(const String &name);

        void registryGlobalFunc(const String &name, NativeFunction *func) const;

        [[nodiscard]] CallFrame *callStack() const;

        [[nodiscard]] size_t &stackTop() const;

        [[nodiscard]] Runtime &rt() const;

        [[nodiscard]] vm::FastRegisterPool &regPool() const;

        [[nodiscard]] GlobalObject *global() const;

        [[nodiscard]] PreProcessor &pp() const;

    private:
        struct Impl;
        Impl *_impl{};

        NativeRegister &nativeRegister();
        [[nodiscard]] const NativeRegister &nativeRegister() const;
    };
} // namespace cial
