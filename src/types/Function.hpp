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

#include <bit>
#include <type_traits>

#include "NativeFunDetail.hpp"
#include "Object.hpp"
#include "Value.hpp"
#include "runtime/Runtime.hpp"
#include "vm/Constant.hpp"

namespace cial {
    struct FuncMeta;
    class VM;

    class Function final : public Object {
    public:
        FuncMeta *meta;
        Object *thisObj{};

        explicit Function(FuncMeta *funcMeta) : Object(Runtime::ATOM_FUNCTION), meta(funcMeta) {}

        // 脚本函数专有标识（NativeFunction 不覆写、保持 Object）：CallV 直写路径
        // 据此一次虚调用判型后 static_cast，替代热路径 dynamic_cast
        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::Function; }

        // instanceof "Function"（TJS2 tTJSInterCodeContext::IsInstanceOf，
        // tjsInterCodeExec.cpp:3023——ctFunction/ctExprFunction 恒真）
        bool instanceOf(const Atom a) override { return a == Runtime::ATOM_FUNCTION; }

        void call(vm::VMState &vmState, u32 ret, size_t argCount) override;

        void marked() noexcept override {
            if(_marked)
                return;
            Object::marked();
            meta->marked();
            if(thisObj)
                thisObj->marked();
        }
    };

    using NativeFn = std::function<Value(VM *vm, Value thisVal, size_t argCount, Value *args)>;

    class NativeFunction final : public Object {
    public:
        // 无捕获回调快路径（small closure：{trampoline, thunkCtx} 两指针直派发）。
        // stdlib 注册面（&stdlib::fn / 无捕获 lambda）全部走此路径：接收者以
        // const 引用跨 ABI 直传（零拷贝），免 std::function 调度链；trampoline
        // 在构造点按 callable 类型静态实例化（native::nativeThinTrampoline）。
        // 有捕获回调保持 NativeFn 按值协议不变（_thin 为空即回落）。
        using NativeThinFn = Value (*)(void *thunkCtx, VM *vm, const Value &thisObj, size_t argCount, Value *args);

        NativeFunction() = delete;

        NativeFunction(NativeFunction &&) = delete;

        // 专有 TypeId：CallV/CallMethodV 用一次虚调用判型走原生同步快路径
        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::NativeFunction; }

        NativeFunction(const NativeFunction &rhs) noexcept :
            Object(Runtime::ATOM_FUNCTION), _thin(rhs._thin), _thunkCtx(rhs._thunkCtx), _arity(rhs._arity),
            _thisObj(rhs._thisObj), _callback(rhs._callback) {}

        template <typename Callable>
            requires(!std::same_as<std::decay_t<Callable>, NativeFunction>)
        explicit NativeFunction(Callable &&callable) :
            Object(Runtime::ATOM_FUNCTION), _arity(native::NativeFunTraits<std::decay_t<Callable>>::arity) {
            using C = std::decay_t<Callable>;
            if constexpr(std::is_pointer_v<C> && std::is_function_v<std::remove_pointer_t<C>>) {
                _thin = &native::nativeThinTrampoline<C>;
                _thunkCtx = std::bit_cast<void *>(callable);
            } else if constexpr(std::is_empty_v<C> && std::is_default_constructible_v<C>) {
                _thin = &native::nativeThinTrampoline<C>; // 无捕获 lambda：trampoline 内默认构造
            } else {
                // 有捕获：保持原 std::function 按值协议（invokeCallable 包装同时
                // 把 void 返回适配为 Value，与旧实现一致）
                _callback = [callable = std::forward<Callable>(callable)](VM *vm, Value thisObj, size_t argCount,
                                                                          Value *args) {
                    return native::invokeCallable(callable, vm, thisObj, argCount, args);
                };
            }
        }

        Value callProc(VM *vm, const size_t argCount, Value *args) const {
            if(_thin)
                return _thin(_thunkCtx, vm, _thisObj, argCount, args);
            return _callback(vm, _thisObj, argCount, args);
        }

        void setThisObj(const Value &thisObj) { _thisObj = thisObj; }

        void call(vm::VMState &vmState, u32 ret, size_t argCount) override;

        void callWithThis(vm::VMState &vmState, u32 ret, size_t argCount, const Value &receiver);

        // 免池直调：args 由调用方准备（栈数组，缺省参数已补位到 _arity），
        // 结果返回给调用方写回，不经池顶窗口也不写寄存器
        Value callDirect(vm::VMState &vmState, const Value &receiver, size_t argCount, Value *args) const;

        [[nodiscard]] size_t arity() const noexcept { return _arity; }

        // 共享原型上的绑定接收者（GMethodC 缓存命中的裸方法无绑定，为 void；
        // propGetMember 现场生成的绑定方法持有接收者）
        [[nodiscard]] const Value &boundThis() const noexcept { return _thisObj; }

        void marked() noexcept override {
            if(_marked)
                return;
            Object::marked();
            if(_thisObj.isObject())
                _thisObj.asObject().value()->marked();
        }

        ~NativeFunction() noexcept override = default;

    private:
        // 布局：热字段在前（callProc 直派发读 _thin/_thunkCtx/_thisObj），
        // 冷的 std::function（32B）垫尾
        NativeThinFn _thin{}; // 非空 → 无捕获快路径
        void *_thunkCtx{}; // trampoline 状态（函数指针值）；空闭包为 null
        size_t _arity;
        Value _thisObj{};
        NativeFn _callback{}; // 有捕获回调（按值协议不变）；无捕获为空
    };

} // namespace cial
