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

#include "Object.hpp"
#include "Value.hpp"
#include "vm/Register.hpp"

namespace cial {
    class Function;
    class Property final : public Object {
    public:
        Value value{};
        Object *thisObj{};
        PropMeta *propMeta{};

        explicit Property(Object *thisObj, PropMeta *propMeta) : thisObj{ thisObj }, propMeta{ propMeta } {}

        void call(Bytecode::VMState &vmState, Bytecode::Register ret, size_t argCount) override {
            throw std::runtime_error("Not Support call operator for property");
        }
    };

} // namespace cial
