#pragma once

#include <string>
#include <string_view>
#include "common/Result.hpp"

namespace cial::format {
    struct Options {
        unsigned indentWidth = 4;
    };

    // 仅改词素之间的空白；字面量、注释正文和预处理区域保持原文，不执行脚本或预处理器。
    // 失败时不修改 output。输出再次词法扫描并核对原始词素；不是完整语法检查器。
    bool formatSource(std::string_view source, std::string &output, Common::Result &result, const Options &options = {},
                      std::string sourceName = "<input>");
} // namespace cial::format
