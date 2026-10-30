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

#include <ranges>

#include "parser/PreProcessor.hpp"
#include "runtime/Runtime.hpp"
#include "stdlib/NativeRegister.hpp"
#include "vm/FastRegisterPool.hpp"

namespace cial {
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
            auto object = rt().create<T>(std::forward<Args>(args)...);
            registerGlobal(name, Value{ object.get() });
            return object.get();
        }

        template <typename T, typename Callable>
        void registerMethod(const String &name, Callable &&fn) {
            return nativeRegister().registerMethod<T>(name, std::forward<Callable>(fn));
        }

        template <typename T>
        void registerProperty(const String &name, const NativePropGetter getter) {
            return nativeRegister().registerProperty<T>(name, getter);
        }

        [[nodiscard]] NativeFunction *findMethod(const TypeId typeId, const Atom a) const {
            return nativeRegister().findMethod(typeId, a);
        }

        [[nodiscard]] NativePropGetter findProperty(const TypeId typeId, const Atom a) const {
            return nativeRegister().findProperty(typeId, a);
        }

        [[nodiscard]] u64 nativeRevision() const { return nativeRegister().revision; }

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
