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

#include "Value.hpp"
#include "gc/gc.hpp"
#include "runtime/AtomTable.hpp"

namespace cial::Bytecode {
    class VMState;
    class Register;
} // namespace cial::Bytecode

namespace cial {
    class Object : public MarkSweepHeader {
        friend class Context;

    public:
        explicit Object() = default;
        explicit Object(const Atom name) : _name{ name } {}

        ~Object() noexcept override = default;

        [[nodiscard]] Atom getName() const noexcept { return _name; }

        void setName(const Atom atom) noexcept { _name = atom; }

        virtual void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) = 0;

        virtual bool instanceOf(Atom a) { return false; }

        virtual void setProp(Atom a, const Value &v) { throw std::runtime_error("Not support"); }

        virtual Value getProp(Atom a) { throw std::runtime_error("Not support"); }

        [[nodiscard]] virtual bool hasProp(Atom a) const { throw std::runtime_error("Not support"); }

    private:
        Atom _name = ATOM_INVALID;
    };

} // namespace cial
