// Copyright (c) 2025/9/29 16:33
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

#include "types/Function.hpp"

namespace cial::stdlib {
    static NativeFunction S_PrintFunction{ [](VM *, const Value &, size_t, Value val) { fmt::print("{}", val); } };

    static NativeFunction S_PrintlnFunction{ [](VM *, const Value &, size_t, Value val) { fmt::println("{}", val); } };
} // namespace cial::stdlib
