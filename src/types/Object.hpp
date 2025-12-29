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

#include "gc/GC.hpp"
#include "parser/AtomTable.hpp"

namespace Cial::Bytecode {
    class VMState;
    class Register;
} // namespace Cial::Bytecode

namespace Cial {
    class Object : public MarkSweepHeader {
    public:
        explicit Object() = default;
        explicit Object(const Atom name) : _name{ name } {}

        ~Object() noexcept override = default;

        [[nodiscard]] Atom getName() const noexcept { return _name; }

        void setName(const Atom atom) noexcept { _name = atom; }

        virtual void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) = 0;

        template <class R, class... Args>
            requires is_gc_object_v<R>
        static Value create(Args... args) {
            return Value{ new R(std::forward<Args>(args)...) };
        }

    private:
        Object *context = nullptr;
        Atom _name = ATOM_INVALID;
    };

} // namespace Cial
