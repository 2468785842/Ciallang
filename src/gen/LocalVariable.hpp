// Copyright (c) 2025/12/31 下午8:33
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

#include "Label.hpp"
#include "Register.hpp"
#include "runtime/AtomTable.hpp"

namespace cial::Inter {

    struct LocalVariable {
        Atom identifier = ATOM_INVALID;
        Register reg{};
        // when (var.startPC <= inst.pc) you can use
        Label startPC{}; // Effective start PC
        // when (var.endPC > inst.pc) you can't use
        Label endPC{}; // Invalid PC (scope ended)
    };
} // namespace cial::Inter