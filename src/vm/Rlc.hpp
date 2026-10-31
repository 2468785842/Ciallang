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

#include <filesystem>


#include "common/SourceLocation.hpp"

namespace cial::vm {
    class Rlc {
    public:
        Rlc() = default;
        explicit Rlc(std::filesystem::path path) : _path(std::move(path)) {}

        void addBytecodeLine(size_t bytecodeIndex, Common::SourceLocation sourceLine) {
            if(!_bytecodeMapLine.empty()) {
                if(_bytecodeMapLine.back().first == bytecodeIndex) {
                    _bytecodeMapLine.back().second = sourceLine;
                    return;
                }
                if(_bytecodeMapLine.back().second == sourceLine)
                    return;
            }
            _bytecodeMapLine.emplace_back(bytecodeIndex, sourceLine);
        }

        [[nodiscard]] bool firstAppear(size_t bytecodeIndex) const;

        [[nodiscard]] Common::SourceLocation find(size_t bytecodeIndex) const;

        [[nodiscard]] bool contains(Common::SourceLocation sourceLine) const;

        [[nodiscard]] std::string name() const { return _path.filename().generic_string(); }

        void reset() { _bytecodeMapLine.clear(); }

        [[nodiscard]] bool empty() const noexcept { return _bytecodeMapLine.empty(); }

        // 换算用：暴露条目（Chunk::toBytecode 把 TAC 下标替换为 bc 偏移）
        [[nodiscard]] std::vector<std::pair<size_t, Common::SourceLocation>> &rawEntries() { return _bytecodeMapLine; }
        [[nodiscard]] const auto &rawEntries() const noexcept { return _bytecodeMapLine; }

    private:
        // 行默认是按序插入, from 0 start
        std::vector<std::pair<size_t, Common::SourceLocation>> _bytecodeMapLine;
        std::filesystem::path _path;
    };
} // namespace cial::vm
