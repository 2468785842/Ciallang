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

#include "TjsTypes.hpp"
#include "TjsValue.hpp"

namespace Ciallang {
    class TjsOctet {
        std::vector<uint8_t> _buf;

    public:
        explicit TjsOctet(std::vector<uint8_t> buf) : _buf(std::move(buf)) {
        }

        static TjsValue tjsOctet(const std::vector<uint8_t>&);
    };


    template <>
    inline void TjsOctetHelper::copy(const TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
        dest._value._octet = new TjsOctet(*src._value._octet);
    }

    template <>
    inline void TjsOctetHelper::move(TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
        dest._value._octet = src._value._octet;
        src._value._octet = nullptr;
    }

    template <>
    inline void TjsOctetHelper::destroy(TjsValue& value) const {
        delete value._value._octet;
    }

}
