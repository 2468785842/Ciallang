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
#include "Rlc.hpp"

namespace Ciallang::VM {
    // 字节码索引是否是这行的第一个
    [[nodiscard]] bool Rlc::firstAppear(const size_t bytecodeIndex) const {
        return std::ranges::any_of(
            _bytecodeMapLine.begin(), _bytecodeMapLine.end(),
            [&](const auto& lineMap) {
                return bytecodeIndex == lineMap.first;
            });
    }

    [[nodiscard]] Common::SourceLocation Rlc::find(const size_t bytecodeIndex) const {
        if(_bytecodeMapLine.empty()) {
            LOG(FATAL) << "rlc is empty";
        }

        Common::SourceLocation preSIndex{};

        for(auto [bcIndex, sIndex] : _bytecodeMapLine) {
            preSIndex = sIndex;
            if(bytecodeIndex == bcIndex) return sIndex;
            if(bytecodeIndex < bcIndex) return preSIndex;
        }

        throw std::logic_error(
            "chunk bytecode map to source line exception, not found"
        );
    }

    [[nodiscard]] bool Rlc::contains(const Common::SourceLocation sourceLine) const {
        if(_bytecodeMapLine.empty()) return false;

        for(size_t i = _bytecodeMapLine.size() - 1; i != 0; i--) {
            auto tmpSourceLine = _bytecodeMapLine.at(i).second;
            if(sourceLine == tmpSourceLine) return true;
        }

        return _bytecodeMapLine.at(0).second == sourceLine;
    }
}
