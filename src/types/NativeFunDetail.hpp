//
// Created by LiDong on 2026/1/4.
//
#pragma once

#include "Octet.hpp"
#include "String.hpp"
#include "Value.hpp"

namespace cial {
    class VM;
}

// 函数特性萃取
namespace cial::NativeFunDetail {
    template <typename T>
    struct NativeFunTraits;

    template <typename Ret, typename... Args>
    struct NativeFunTraits<Ret (*)(Args...)> {
        using ReturnType = Ret;
        using ArgsTuple = std::tuple<Args...>;
        static constexpr size_t arity = sizeof...(Args);
        template <size_t N>
        using Arg = std::tuple_element_t<N, ArgsTuple>;
    };

    template <typename Ret, typename... Args>
    struct NativeFunTraits<Ret (*)(VM *, const Value &, size_t, Args...)> : NativeFunTraits<Ret (*)(Args...)> {};

    template <typename Ret, typename... Args>
    struct NativeFunTraits<Ret (*)(VM *, Value, size_t, Args...)> : NativeFunTraits<Ret (*)(Args...)> {};

    // 成员函数指针（非 const）
    template <typename C, typename Ret, typename... Args>
    struct NativeFunTraits<Ret (C::*)(Args...)> : NativeFunTraits<Ret (*)(Args...)> {};

    // 成员函数指针（const）
    template <typename C, typename Ret, typename... Args>
    struct NativeFunTraits<Ret (C::*)(Args...) const> : NativeFunTraits<Ret (*)(Args...)> {};

    // 成员函数指针（可变 lambda，对应 operator()&
    template <typename C, typename Ret, typename... Args>
    struct NativeFunTraits<Ret (C::*)(Args...) &> : NativeFunTraits<Ret (*)(Args...)> {};

    // 成员函数指针（const 可变组合）
    template <typename C, typename Ret, typename... Args>
    struct NativeFunTraits<Ret (C::*)(Args...) const &> : NativeFunTraits<Ret (*)(Args...)> {};

    // lambda / 仿函数
    template <typename F>
    struct NativeFunTraits : NativeFunTraits<std::decay_t<decltype(&F::operator())>> {};

    template <typename T, typename U>
    inline constexpr bool is_value_ptr_v = std::is_pointer_v<std::decay_t<T>> &&
        std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>, U>;

    template <typename T>
    T extractArg(Value *args, const size_t index) {
        // 根据T类型从args[index]中提取实际值
        if constexpr(std::is_same_v<std::decay_t<T>, Integer>) {
            return args[index].toInteger();
        } else if constexpr(std::is_same_v<std::decay_t<T>, Real>) {
            return args[index].toReal();
        } else if constexpr(std::is_same_v<std::decay_t<T>, bool>) {
            return args[index].toBool();
        } else if constexpr(is_value_ptr_v<T, String>) {
            return args[index].toString();
        } else if constexpr(is_value_ptr_v<T, Object>) {
            return args[index].toObject();
        } else if constexpr(is_value_ptr_v<T, Octet>) {
            return args[index].toOctet();
        } else if constexpr(std::is_pointer_v<std::decay_t<T>>) {
            return dynamic_cast<T *>(args[index].toObject());
        } else {
            return args[index];
        }
    }

    template <typename Ret>
    Value wrapToValue(Ret &&ret) {
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

    template <typename Callable, typename Tuple, typename Args, size_t... I>
    Value invokeCallableImpl(Callable &&callable, VM *vm, Value thisObj, size_t argCount, Args args,
                             std::index_sequence<I...>) {
        using Traits = NativeFunTraits<std::decay_t<Callable>>;
        using Ret = Traits::ReturnType;

        auto invoke = [&]() -> Ret {
            if constexpr(Traits::arity == 0) {
                return callable(vm, thisObj, argCount);
            } else {
                using FirstArg = Traits::template Arg<0>;
                if constexpr(is_value_ptr_v<FirstArg, Value>) {
                    return callable(vm, thisObj, argCount, args);
                } else {
                    return callable(vm, thisObj, argCount, extractArg<std::tuple_element_t<I, Tuple>>(args, I)...);
                }
            }
        };

        if constexpr(std::is_void_v<Ret>) {
            invoke();
            return Value{};
        } else {
            return wrapToValue(invoke());
        }
    }

    template <typename Callable, typename Args>
    Value invokeCallable(Callable &&callable, VM *vm, Value thisObj, size_t argCount, Args args) {
        using Traits = NativeFunTraits<std::decay_t<Callable>>;
        using Tuple = Traits::ArgsTuple;
        constexpr size_t N = Traits::arity;
        return invokeCallableImpl<Callable, Tuple>(std::forward<Callable>(callable), vm, thisObj, argCount, args,
                                                   std::make_index_sequence<N>{});
    }

} // namespace cial::NativeFunDetail