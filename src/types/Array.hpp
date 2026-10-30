// Copyright (c) 2026/10/24
//
// Array 内建容器（TJS2 Array）：整数下标的动态数组。
// 成员访问协议：count/length 属性可读写（写=调整长度，扩补 void/缩截断），
// 方法经 NativeRegister 的 TypeId::Array 表分派（stdlib::ArrayLib 注册）。
#pragma once

#include "Object.hpp"
#include "Value.hpp"
#include "runtime/Runtime.hpp"

namespace cial {
    class Runtime;

    class Array final : public Object {
    public:
        Vec<Value> elements{};
        Runtime *rt{};

        explicit Array() = delete;

        explicit Array(Runtime *runtime) : rt(runtime) {}

        explicit Array(Runtime *runtime, const size_t count) : rt(runtime) { elements.resize(count); }

        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::Array; }

        // instanceof "Array"（TJS2 原生类链按名匹配，tjsArray 派生自 Array 类）
        bool instanceOf(const Atom a) override { return a == Runtime::ATOM_ARRAY; }

        void call(vm::VMState &vmState, u32 ret, size_t argCount) override;

        void marked() noexcept override {
            if(_marked)
                return;
            Object::marked();
            for(auto &v : elements) {
                if(v.isObject()) {
                    if(auto *h = v.asObject().unwrap()) {
                        h->marked();
                    }
                }
            }
        }

        void setProp(Atom a, const Value &v) override;

        Value getProp(Atom a) override;

        [[nodiscard]] bool hasProp(Atom a) const override;

        [[nodiscard]] bool delProp(Atom a) override;

        [[nodiscard]] bool isCountAtom(Atom a) const;
    };

} // namespace cial
