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
#include "Value.hpp"

#include <ranges>

#include "Function.hpp"

namespace Ciallang {
    ClassObject::~ClassObject() noexcept {
        if(_base) {
            _base->decRef();
        }

        for(const auto &v : _methods | std::views::values) {
            delete v;
        }
    }

    void ClassObject::setBase(ClassObject *base) noexcept {
        if(_base) {
            _base->decRef();
        }
        _base = base;
        if(_base) {
            _base->incRef();
        }
    }

    void ClassObject::setMethod(const std::string &name, const Value &fun) noexcept {
        _methods[name] = new Value{ fun };
    }

    Value ClassObject::getMethod(const std::string &name) const noexcept {
        const auto it = _methods.find(name);
        return it != _methods.end() ? *it->second : Value{};
    }

    [[nodiscard]] std::vector<Value> ClassObject::getAllMethods() const noexcept {
        std::vector<Value> result;
        for(auto &method : _methods | std::views::values) {
            result.push_back(*method);
        }
        return result;
    }
} // namespace Ciallang
