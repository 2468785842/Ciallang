/*
 * Copyright (c) 2024/5/29 下午5:45
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

#include "pch.h"
#include "common/SourceLocation.hpp"

namespace Ciallang::Bytecode {
    class Rlc {
    public:
        explicit Rlc(std::filesystem::path path) : _path(std::move(path)) {
        }

        void addBytecodeLine(size_t bytecodeIndex, Common::SourceLocation sourceLine) {
            if(contains(sourceLine)) {
                _bytecodeMapLine.pop_back();
            }
            _bytecodeMapLine.emplace_back(bytecodeIndex, sourceLine);
        }

        [[nodiscard]] bool firstAppear(size_t bytecodeIndex) const;

        [[nodiscard]] Common::SourceLocation find(size_t bytecodeIndex) const;

        [[nodiscard]] bool contains(Common::SourceLocation sourceLine) const;

        [[nodiscard]] std::string name() const { return _path.filename().generic_string(); }

        void reset() { _bytecodeMapLine.clear(); }

    private:
        // 行默认是按序插入, from 0 start
        std::vector<std::pair<size_t, Common::SourceLocation>> _bytecodeMapLine;
        std::filesystem::path _path;
    };
}
