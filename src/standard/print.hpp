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

//
// Created by LiDon on 2025/9/29.
//
#pragma once

#include "pch.h"

#include "types/TjsNativeFunction.hpp"

namespace Ciallang::Standard {
    static TjsNativeFunction S_PrintFunction{ [](const TjsValue *values) { fmt::print("{}", *values); }, 1,
                                                    "print" };

    static TjsNativeFunction S_PrintlnFunction{ [](const TjsValue *values) { fmt::println("{}", *values); }, 1,
                                                      "println" };
} // namespace Ciallang::Standard
