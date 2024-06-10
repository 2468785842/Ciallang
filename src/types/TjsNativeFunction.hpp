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

#include "TjsObject.hpp"

namespace Ciallang {
    class TjsNativeFunction final : public TjsObject {
        using Callback = std::function<TjsValue(TjsValue*)>;

    public:
        TjsNativeFunction() = delete;

        explicit TjsNativeFunction(
            const Callback& callback,
            const size_t arity,
            std::string&& name
        ): _callback(callback), _arity(arity), _name(name) {
        }

        explicit TjsNativeFunction(
            const Callback& callback,
            const size_t arity,
            const std::string& name
        ): _callback(callback), _arity(arity), _name(name) {
        }

        TjsValue callProc(TjsValue* values) const {
            return std::move(_callback(values));
        }

        [[nodiscard]] std::string_view name() const noexcept override {
            return _name;
        }

        [[nodiscard]] size_t arity() const noexcept override {
            return _arity;
        }

        [[nodiscard]] bool isNative() const noexcept override {
            return true;
        }

        [[nodiscard]] std::unique_ptr<TjsObject> clone() const noexcept override {
            return std::make_unique<TjsNativeFunction>(
                _callback, _arity, _name
            );
        }

        ~TjsNativeFunction() noexcept override = default;

    private:
        const Callback _callback;
        const size_t _arity;
        const std::string _name;
    };
}
