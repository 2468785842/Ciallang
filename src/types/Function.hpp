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
        NativeFunction() = delete;

        NativeFunction(NativeFunction &&) = delete;

        // 专有 TypeId：CallV/CallMethodV 用一次虚调用判型走原生同步快路径
        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::NativeFunction; }

        NativeFunction(const NativeFunction &rhs) noexcept :
            Object(Runtime::ATOM_FUNCTION), _callback(rhs._callback), _arity(rhs._arity) {}

        template <typename Callable>
            requires(!std::same_as<std::decay_t<Callable>, NativeFunction>)
        explicit NativeFunction(Callable &&callable) :
            Object(Runtime::ATOM_FUNCTION), _callback([callable = std::forward<Callable>(callable)](
                                                          VM *vm, Value thisObj, size_t argCount, Value *args) {
                return native::invokeCallable(callable, vm, thisObj, argCount, args);
            }),
            _arity(native::NativeFunTraits<Callable>::arity) {}

        Value callProc(VM *vm, const size_t argCount, Value *args) const {
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
        const NativeFn _callback;
        const size_t _arity;
        Value _thisObj{};
    };

} // namespace cial
