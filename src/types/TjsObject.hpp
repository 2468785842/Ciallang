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

#include "TjsTypes.hpp"
#include "TjsValue.hpp"

namespace Ciallang {

    class TjsObject {
    public:

        virtual ~TjsObject() noexcept = default;

        [[nodiscard]] virtual std::string_view name() const noexcept = 0;

        [[nodiscard]] virtual std::unique_ptr<TjsObject> clone() const noexcept = 0;

        [[nodiscard]] virtual bool isNative() const noexcept = 0;

        [[nodiscard]] virtual size_t arity() const noexcept = 0;
    };

    template <>
    inline void TjsObjectHelper::copy(const TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
        dest._value._object = src._value._object->clone().release();
    }

    template <>
    inline void TjsObjectHelper::move(TjsValue& src, TjsValue& dest) const {
        dest._type = src._type;
        dest._value._object = src._value._object;
        src._value._object = nullptr;
    }

    template <>
    inline void TjsObjectHelper::destroy(TjsValue& value) const {
        delete value._value._object;
    }
}
