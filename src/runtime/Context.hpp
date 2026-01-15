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
#include "stdlib/NativeRegister.hpp"
#include "vm/FastRegisterPool.hpp"

namespace cial {
    class GlobalObject;
    struct CallFrame;

    class Context {
    public:
        // Stack Max Depth Is 1024
        static constexpr auto maxCallDepth = 1024;

        explicit Context(Runtime &rt) noexcept;
        ~Context() noexcept;

        void initNativeMethod();

        template <typename T, typename Callable>
        void registerMethod(const String &name, Callable &&fn) {
            return nativeRegister().registerMethod<T>(name, std::forward<Callable>(fn));
        }

        [[nodiscard]] NativeFunction *findMethod(const TypeId typeId, const Atom a) const {
            return nativeRegister().findMethod(typeId, a);
        }

        void registryGlobalFunc(const String &name, NativeFunction *func) const noexcept;

        void collectMark() const;

        CallFrame *callStack() const;

        size_t &stackTop() const;

        Runtime &rt() const;

        Bytecode::FastRegisterPool &regPool() const;

        GlobalObject *global() const;

        PreProcessor &pp() const;

    private:
        struct Impl;
        Impl *_impl{};

        NativeRegister &nativeRegister();
        const NativeRegister &nativeRegister() const;
    };
} // namespace cial