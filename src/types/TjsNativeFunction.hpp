/*
 * Copyright (c) 2024/6/10 下午5:55
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

#include "pch.h"

#include "TjsObject.hpp"
#include "TjsValue.hpp"

namespace Ciallang {

    class TjsNativeFunction final : public TjsObject {
        using Callback = std::function<TjsValue(const TjsValue *)>;
        using CallbackVoid = std::function<void(const TjsValue *)>;

    public:
        TjsNativeFunction() = delete;

        explicit TjsNativeFunction(const Callback &callback, size_t arity, std::string name);

        explicit TjsNativeFunction(const CallbackVoid &callback, size_t arity, std::string name);

        TjsValue callProc(const TjsValue *values) const { return _callback(values); }

        [[nodiscard]] const char *name() const noexcept override { return _name.c_str(); }

        [[nodiscard]] size_t arity() const noexcept override { return _arity; }

        ~TjsNativeFunction() noexcept override = default;

    private:
        const Callback _callback;
        const size_t _arity;
        const std::string _name;
    };
} // namespace Ciallang
