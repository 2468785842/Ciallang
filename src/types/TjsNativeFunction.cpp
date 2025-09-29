// Copyright (c) 2025/9/29 15:50
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

#include "TjsNativeFunction.hpp"

namespace Ciallang {

    TjsNativeFunction::TjsNativeFunction(const Callback &callback, const size_t arity, std::string name) :
        TjsObject(true), _callback(callback), _arity(arity), _name(std::move(name)) {}

    TjsNativeFunction::TjsNativeFunction(const CallbackVoid &callback, const size_t arity, std::string name) :
        TjsObject(true), _callback([callback](const TjsValue *values) -> TjsValue {
            // just warp
            callback(values);
            return {};
        }),
        _arity(arity), _name(std::move(name)) {}

} // namespace Ciallang