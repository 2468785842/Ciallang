// Copyright (c) 2026/10/24
//
// RegExp 内建对象（TJS2 RegExp）：/pattern/ 字面量的运行时载体。
// 引擎用 std::regex（ECMAScript 方言；TJS2 原版为 oniguruma，复杂模式差异记 TODO）。
#pragma once

#include <regex>

#include "Object.hpp"
#include "Value.hpp"

namespace cial {

    class RegExp final : public Object {
    public:
        std::string pattern{};
        std::regex re{};

        explicit RegExp() = delete;

        explicit RegExp(Runtime *rt, const String &patternStr) :
            pattern(patternStr.getData(), patternStr.length()) {
            try {
                re = std::regex(pattern, std::regex::ECMAScript);
            } catch(const std::regex_error &e) {
                throw std::runtime_error("invalid regex /" + pattern + "/: " + e.what());
            }
            (void)rt;
        }

        void call(vm::VMState &vmState, u32 ret, size_t argCount) override {
            throw std::runtime_error("Not implemented RegExp call");
        }
    };

} // namespace cial
