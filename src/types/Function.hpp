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
#include "vm/Constant.hpp"

namespace cial {
    struct FuncMeta;
    class VM;

    class Function final : public Object {
    public:
        FuncMeta *meta;
        Object *thisObj{};

        explicit Function(FuncMeta *funcMeta) : Object(ATOM_FUNCTION), meta(funcMeta) {}

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        void marked() noexcept override {
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

        NativeFunction(const NativeFunction &rhs) noexcept :
            Object(ATOM_FUNCTION), _callback(rhs._callback), _arity(rhs._arity) {}

        template <typename Callable>
            requires(!std::same_as<std::decay_t<Callable>, NativeFunction>)
        explicit NativeFunction(Callable &&callable) :
            Object(ATOM_FUNCTION), _callback([callable = std::forward<Callable>(callable)](
                                                 VM *vm, Value thisObj, size_t argCount, Value *args) {
                return NativeFunDetail::invokeCallable(callable, vm, thisObj, argCount, args);
            }),
            _arity(NativeFunDetail::NativeFunTraits<Callable>::arity) {}

        Value callProc(VM *vm, const size_t argCount, Value *args) const {
            return _callback(vm, _thisObj, argCount, args);
        }

        void setThisObj(const Value &thisObj) { _thisObj = thisObj; }

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override;

        void marked() noexcept override {
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
