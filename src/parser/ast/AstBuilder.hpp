/*
 * Copyright (c) 2024/5/6 下午8:16
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

#include "AstNode.hpp"

namespace cial::syntax {
    class AstBuilder {
        std::vector<AstNode *> _nodes{};

    public:
        AstBuilder() = default;

        ~AstBuilder() {
            for(const auto &val : _nodes) {
                delete val;
            }
        }

        template <typename R, typename... Args>
        R *makeNode(Args &&...args) {
            static_assert(std::is_base_of_v<AstNode, R>, "Error: R must be a derived class of AstNode.");

            R *node = new R{ std::forward<Args>(args)... };
            _nodes.push_back(node);
            return node;
        }
    };
} // namespace cial::syntax
