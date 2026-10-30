// Copyright (c) 2026/10/24
//
// Dictionary 内建容器（TJS2 Dictionary）：字符串键的哈希字典。
// 键统一 Atom 化（下标写入时 toString + intern）；count 属性只读；
// 方法经 NativeRegister 的 TypeId::Dictionary 表分派（stdlib::DictLib 注册）。
#pragma once

#include <ranges>

#include "Object.hpp"
#include "Value.hpp"
#include "runtime/Runtime.hpp"

namespace cial {

    class Dictionary final : public Object {
    public:
        Map<Atom, Value> members{};

        explicit Dictionary() = default;

        [[nodiscard]] TypeId typeId() const noexcept override { return TypeId::Dictionary; }

        // instanceof "Dictionary"（TJS2 原生类链按名匹配）
        bool instanceOf(const Atom a) override { return a == Runtime::ATOM_DICTIONARY; }

        void call(vm::VMState &vmState, u32 ret, size_t argCount) override;

        void marked() noexcept override {
            if(_marked)
                return;
            Object::marked();
            for(auto &v : members | std::views::values) {
                if(v.isObject()) {
                    if(auto *h = v.asObject().unwrap()) {
                        h->marked();
                    }
                }
            }
        }

        void setProp(Atom a, const Value &v) override { members[a] = v; }

        Value getProp(Atom a) override {
            if(const auto it = members.find(a); it != members.end())
                return it->second;
            throw std::runtime_error("in dictionary Not found member");
        }

        [[nodiscard]] bool hasProp(Atom a) const override { return members.contains(a); }

        [[nodiscard]] bool delProp(Atom a) override { return members.erase(a) > 0; }
    };

} // namespace cial
