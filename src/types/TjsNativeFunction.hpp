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

#include <tuple>

#include "TjsObject.hpp"
#include "TjsString.hpp"
#include "TjsValue.hpp"

namespace Ciallang {

    // 函数特性萃取
    namespace detail {
        template <typename T>
        struct function_traits;

        template <typename Ret, typename... Args>
        struct function_traits<Ret (*)(Args...)> {
            using return_type = Ret;
            using args_tuple = std::tuple<Args...>;
            static constexpr size_t arity = sizeof...(Args);
            template <size_t N>
            using arg = std::tuple_element_t<N, args_tuple>;
        };

        // 成员函数指针（非 const）
        template <typename C, typename Ret, typename... Args>
        struct function_traits<Ret (C::*)(Args...)> : function_traits<Ret (*)(Args...)> {};

        // 成员函数指针（const）
        template <typename C, typename Ret, typename... Args>
        struct function_traits<Ret (C::*)(Args...) const> : function_traits<Ret (*)(Args...)> {};

        // 成员函数指针（可变 lambda，对应 operator()&）
        template <typename C, typename Ret, typename... Args>
        struct function_traits<Ret (C::*)(Args...) &> : function_traits<Ret (*)(Args...)> {};

        // 成员函数指针（const 可变组合）
        template <typename C, typename Ret, typename... Args>
        struct function_traits<Ret (C::*)(Args...) const &> : function_traits<Ret (*)(Args...)> {};

        // lambda / 仿函数
        template <typename F>
        struct function_traits : function_traits<decltype(&F::operator())> {};

    } // namespace detail

    class TjsNativeFunction final : public TjsObject {
        using Callback = std::function<TjsValue(TjsValue *)>;

    public:
        TjsNativeFunction() = delete;

        // 可变参数版本 - 自动推导参数个数
        template <typename Callable>
        static TjsNativeFunction create(std::string_view name, Callable &&callable) {
            return TjsNativeFunction{ name, std::forward<Callable>(callable) };
        }
        TjsValue callProc(TjsValue *values) const { return _callback(values); }

        [[nodiscard]] const char *name() const noexcept override { return _name.c_str(); }

        [[nodiscard]] size_t arity() const noexcept override { return _arity; }

        ~TjsNativeFunction() noexcept override = default;

    private:
        template <typename Callable>
        explicit TjsNativeFunction(const std::string_view name, Callable &&callable) :
            TjsObject{ true }, _callback([callable = std::forward<Callable>(callable)](TjsValue *args) {
                return invoke_callable(callable, args);
            }),
            _arity(detail::function_traits<Callable>::arity), _name(name) {}

        template <typename T>
        static T extract_arg(const TjsValue *args, const size_t index) {
            // 根据T类型从args[index]中提取实际值
            if constexpr(std::is_same_v<T, TjsInteger>) {
                return args[index].toInteger();
            } else if constexpr(std::is_same_v<T, TjsReal>) {
                return args[index].toReal();
            } else if constexpr(std::is_same_v<T, bool>) {
                return args[index].toBool();
            } else if constexpr(std::is_same_v<T, TjsString>) {
                return *args[index].toString();
            } else if constexpr(std::is_same_v<T, TjsObject>) {
                return *args[index].toObject();
            } else if constexpr(std::is_same_v<T, TjsOctet>) {
                return *args[index].toOctet();
            } else {
                return args[index];
            }
        }

        template <typename Callable, typename Tuple, size_t... I>
        static TjsValue invoke_callable_impl(Callable &&callable, const TjsValue *args, std::index_sequence<I...>) {
            using Ret = detail::function_traits<std::decay_t<Callable>>::return_type;
            if constexpr(std::is_void_v<Ret>) {
                callable(extract_arg<std::tuple_element_t<I, Tuple>>(args, I)...);
                return TjsValue{};
            } else {
                return callable(extract_arg<std::tuple_element_t<I, Tuple>>(args, I)...);
            }
        }

        template <typename Callable>
        static TjsValue invoke_callable(Callable &&callable, const TjsValue *args) {
            using traits = detail::function_traits<std::decay_t<Callable>>;
            using Tuple = traits::args_tuple;
            constexpr size_t N = traits::arity;
            return invoke_callable_impl<Callable, Tuple>(std::forward<Callable>(callable), args,
                                                         std::make_index_sequence<N>{});
        }

    private:
        const Callback _callback;
        const size_t _arity;
        const std::string _name;
    };

} // namespace Ciallang
