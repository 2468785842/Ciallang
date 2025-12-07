// Copyright (c) 2024/5/21 下午8:44
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

#include "Object.hpp"

#include "Function.hpp"

namespace Ciallang {

    void ClassObject::setMethod(const std::string &name, Function *fun) noexcept {
        auto it = _methods.find(name);
        if(it != _methods.end() && it->second)
            it->second->decRef();
        _methods[name] = fun;
        if(fun)
            fun->incRef();
    }

    Function *ClassObject::getMethod(const std::string &name) const noexcept {
        auto it = _methods.find(name);
        return it != _methods.end() ? it->second : nullptr;
    }
} // namespace Ciallang
