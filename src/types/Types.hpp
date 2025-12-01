// Copyright (c) 2024/5/23 下午9:40
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

#include <cstdint>

namespace Ciallang {
#if defined(LLONG_MAX)
    using Integer = long long;
    using Real = double;
#else
    using Integer = int;
    using Real = float;
#endif

    enum class ValueType : std::uint8_t {
        Void, // empty
        Object,
        String,
        Octet, // octet binary data
        Integer,
        Real
    };

    class String;
    class Octet;
    class Object;

    class Value;

} // namespace Ciallang
