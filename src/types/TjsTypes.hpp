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

namespace Ciallang {

#if defined (LLONG_MAX)
    using TjsInteger = long long;
    using TjsReal = double;
#else
    using TjsInteger = int;
    using TjsReal = float;
#endif

    enum class TjsValueType {
        Void, // empty
        Object,
        String,
        Octet, // octet binary data
        Integer,
        Real
    };

    class TjsString;
    class TjsOctet;
    class TjsObject;

    class TjsValue;
}