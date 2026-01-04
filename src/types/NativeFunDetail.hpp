//
// Created by LiDong on 2026/1/4.
//
#pragma once

#include "Octet.hpp"
#include "String.hpp"
#include "Value.hpp"

namespace Cial {
    class VM;
}

// 函数特性萃取
namespace Cial::NativeFunDetail {
    template <typename T>
    struct function_traits;

    template <typename Ret, typename... Args>
    struct function_traits<Ret (*)(Args...)> {
        using return_type = Ret;
        using args_tuple = std::tuple<Args...>;
        static constexpr size_t arity = sizeof...(Args);
    };

    template <typename Ret, typename... Args>
    struct function_traits<Ret (*)(VM *, const Value &, Args...)> : function_traits<Ret (*)(Args...)> {};

    template <typename Ret, typename... Args>
    struct function_traits<Ret (*)(VM *, Value, Args...)> : function_traits<Ret (*)(Args...)> {};

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
    T extract_arg(Value *args, const size_t index) {
        // 根据T类型从args[index]中提取实际值
        if constexpr(std::is_same_v<T, Integer>) {
            return args[index].toInteger();
        } else if constexpr(std::is_same_v<T, Real>) {
            return args[index].toReal();
        } else if constexpr(std::is_same_v<T, bool>) {
            return args[index].toBool();
        } else if constexpr(std::is_same_v<T, String *>) {
            return args[index].toString();
        } else if constexpr(std::is_same_v<T, Object *>) {
            return args[index].toObject();
        } else if constexpr(std::is_same_v<T, Octet *>) {
            return args[index].toOctet();
        } else if constexpr(std::is_pointer_v<T>) {
            return dynamic_cast<T *>(args[index].toObject());
        } else {
            return args[index];
        }
    }

    template <typename Ret>
    Value wrap_to_value(Ret &&ret) {
        if constexpr(std::is_same_v<std::decay_t<Ret>, Value>) {
            return std::forward<Ret>(ret);
        } else if constexpr(std::is_same_v<std::decay_t<Ret>, String>) {
            return Value{ new String{ std::forward<Ret>(ret) } };
        } else if constexpr(std::is_same_v<std::decay_t<Ret>, Octet>) {
            return Value{ new Octet{ std::forward<Ret>(ret) } };
        } else {
            return Value{ std::forward<Ret>(ret) };
        }
    }

    template <typename Callable, typename Tuple, typename Arg, size_t... I>
    Value invoke_callable_impl(Callable &&callable, VM *vm, Value thisObj, Arg args, std::index_sequence<I...>) {
        using Ret = function_traits<std::decay_t<Callable>>::return_type;

        if constexpr(std::is_void_v<Ret>) {
            callable(vm, thisObj, extract_arg<std::tuple_element_t<I, Tuple>>(args, I)...);
            return Value{};
        } else {
            Ret r = callable(vm, thisObj, extract_arg<std::tuple_element_t<I, Tuple>>(args, I)...);
            return wrap_to_value(std::move(r));
        }
    }

    template <typename Callable, typename Arg>
    Value invoke_callable(Callable &&callable, VM *vm, Value thisObj, Arg args) {
        using traits = function_traits<std::decay_t<Callable>>;
        using Tuple = traits::args_tuple;
        constexpr size_t N = traits::arity;
        return invoke_callable_impl<Callable, Tuple>(std::forward<Callable>(callable), vm, thisObj, args,
                                                     std::make_index_sequence<N>{});
    }

} // namespace Cial::NativeFunDetail