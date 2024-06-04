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
    class TjsString : public std::string {
        // TODO: impl
    public:

        ~TjsString() {
            std::string::~string();
        }

        static TjsValue tjsString(const std::string&);
    };


    template <>
    inline void TjsStringHelper::copy(const TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
        dest._value._string = new TjsString(*src._value._string);
    }

    template <>
    inline void TjsStringHelper::move(TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
        dest._value._string = src._value._string;
        src._value._string = nullptr;
    }

    template <>
    inline void TjsStringHelper::destroy(TjsValue& value) const {
        delete value._value._string;
    }
}
