/*
 * Copyright (c) 2024/6/8 上午10:23
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

#include <tuple>

#include "Object.hpp"
#include "TjsString.hpp"
#include "TjsValue.hpp"
#include "vm/Chunk.hpp"

namespace Ciallang {

    class Function final : public Object {
    public:
        Function() = delete;

        explicit Function(Bytecode::Chunk *chunk, const std::string &name);

        explicit Function(Bytecode::Chunk *chunk, std::string name, size_t arity);

        [[nodiscard]] const char *name() const noexcept override { return _name.c_str(); }

        [[nodiscard]] const Bytecode::Chunk *chunk() const { return _chunk.get(); }

        [[nodiscard]] size_t arity() const noexcept override { return _arity; }
        
        ~Function() noexcept override = default;

    private:
        const std::unique_ptr<Bytecode::Chunk> _chunk;
        const std::string _name;
        const size_t _arity;
    };


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

        template <typename T>
        T extract_arg(TjsValue *args, const size_t index) {
            // 根据T类型从args[index]中提取实际值
            if constexpr(std::is_same_v<T, TjsInteger>) {
                return args[index].toInteger();
            } else if constexpr(std::is_same_v<T, TjsReal>) {
                return args[index].toReal();
            } else if constexpr(std::is_same_v<T, bool>) {
                return args[index].toBool();
            } else {
                return args[index];
            }
        }

        template <>
        inline TjsString *extract_arg<TjsString *>(TjsValue *args, const size_t index) {
            return args[index].toString();
        }

        template <>
        inline Object *extract_arg<Object *>(TjsValue *args, const size_t index) {
            return args[index].toObject();
        }

        template <>
        inline TjsOctet *extract_arg<TjsOctet *>(TjsValue *args, const size_t index) {
            return args[index].toOctet();
        }

        template <typename Callable, typename Tuple, size_t... I>
        TjsValue invoke_callable_impl(Callable &&callable, TjsValue *args, std::index_sequence<I...>) {
            using Ret = function_traits<std::decay_t<Callable>>::return_type;
            if constexpr(std::is_void_v<Ret>) {
                callable(detail::extract_arg<std::tuple_element_t<I, Tuple>>(args, I)...);
                return TjsValue{};
            } else {
                return callable(detail::extract_arg<std::tuple_element_t<I, Tuple>>(args, I)...);
            }
        }

        template <typename Callable>
        TjsValue invoke_callable(Callable &&callable, TjsValue *args) {
            using traits = function_traits<std::decay_t<Callable>>;
            using Tuple = traits::args_tuple;
            constexpr size_t N = traits::arity;
            return detail::invoke_callable_impl<Callable, Tuple>(std::forward<Callable>(callable), args,
                                                                 std::make_index_sequence<N>{});
        }

    } // namespace detail

    class NativeFunction final : public Object {
        using Callback = std::function<TjsValue(TjsValue *)>;

    public:
        NativeFunction() = delete;

        template <typename Callable>
        explicit NativeFunction(const std::string_view name, Callable &&callable) :
             _callback([callable = std::forward<Callable>(callable)](TjsValue *args) {
                return detail::invoke_callable(callable, args);
            }),
            _arity(detail::function_traits<Callable>::arity), _name(name) {}

        TjsValue callProc(TjsValue *values) const { return _callback(values); }

        [[nodiscard]] const char *name() const noexcept override { return _name.c_str(); }

        [[nodiscard]] size_t arity() const noexcept override { return _arity; }

        ~NativeFunction() noexcept override = default;

    private:
        const Callback _callback;
        const size_t _arity;
        const std::string _name;
    };

} // namespace Ciallang
