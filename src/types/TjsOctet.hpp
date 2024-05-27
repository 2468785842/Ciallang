// Copyright (c) 2024/5/23 下午9:43
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
#include <utility>
#include <vector>

#include "TjsTypes.hpp"

namespace Ciallang {
    class TjsOctet {
        std::vector<uint8_t> _buf;
    public:
        explicit TjsOctet(std::vector<uint8_t> buf) : _buf(std::move(buf)){}

        static TjsValue tjsOctet(const std::vector<uint8_t >&);
    };

}