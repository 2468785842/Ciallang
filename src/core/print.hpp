/*
 * Copyright (c) 2024/6/10 下午9:29
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

#include "pch.h"

#include "types/TjsNativeFunction.hpp"

namespace Ciallang::Core {
    static const auto S_PrintFunction = TjsNativeFunction{
            [](TjsValue* values) {
                fmt::print("{}", *values);
            },
            1, "print"
    };

    static const auto S_PrintlnFunction = TjsNativeFunction{
            [](TjsValue* values) {
                fmt::println("{}", *values);
            },
            1, "println"
    };
}
