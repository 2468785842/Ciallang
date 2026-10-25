// Copyright (c) 2024/5/23 下午9:37
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

#pragma once

#include "TypeId.hpp"
#include "Value.hpp"
#include "gc/gc.hpp"
#include "runtime/AtomTable.hpp"

namespace cial::vm {
    class VMState;
    class Register;
} // namespace cial::vm

namespace cial {
    class Object : public MarkSweepHeader {
        friend class Context;

    public:
        explicit Object() = default;
        explicit Object(const Atom name) : _name{ name } {}

        ~Object() noexcept override = default;

        [[nodiscard]] Atom getName() const noexcept { return _name; }

        void setName(const Atom atom) noexcept { _name = atom; }

        // 动态类型标识：一次虚调用替代 NativeRegister 分派处的 dynamic_cast 链
        // （Array/Dictionary/RegExp 等有原生方法表的子类覆写）
        [[nodiscard]] virtual TypeId typeId() const noexcept { return TypeId::Object; }

        virtual void call(vm::VMState &vmState, u32 ret, size_t argCount) = 0;

        virtual bool instanceOf(Atom a) { return false; }

        virtual void setProp(Atom a, const Value &v) { throw std::runtime_error("Not support"); }

        virtual Value getProp(Atom a) { throw std::runtime_error("Not support"); }

        [[nodiscard]] virtual bool hasProp(Atom a) const { throw std::runtime_error("Not support"); }

        // delete obj.member（TJS2 DeleteMember）：返回是否删除了成员
        [[nodiscard]] virtual bool delProp(Atom a) { throw std::runtime_error("Not support"); }

    private:
        Atom _name = ATOM_INVALID;
    };

} // namespace cial
