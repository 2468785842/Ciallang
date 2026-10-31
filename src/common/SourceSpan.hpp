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

#include <limits>

#include "types/Types.hpp"

namespace cial::Common {
    // 源码范围：UTF-8 字节偏移的半开区间 [begin, end)，0 <= begin <= end <= size。
    // begin == end 表示零宽（插入点）；npos 表示未知位置（与文件开头区分）。
    // 行列由 SourceFile 按行索引换算，字节列为权威，UTF-16/终端宽度由消费者转换。
    struct SourceSpan {
        static constexpr size_t npos = std::numeric_limits<size_t>::max();

        size_t begin = npos;
        size_t end = npos;

        constexpr SourceSpan() = default;

        constexpr SourceSpan(const size_t beginByte, const size_t endByte) : begin(beginByte), end(endByte) {}

        [[nodiscard]] constexpr bool valid() const noexcept { return begin != npos && end != npos; }

        [[nodiscard]] constexpr bool empty() const noexcept { return valid() && begin == end; }

        [[nodiscard]] constexpr size_t size() const noexcept { return valid() ? end - begin : 0; }

        constexpr bool operator==(const SourceSpan &span) const noexcept {
            if(!valid() || !span.valid())
                return valid() == span.valid();
            return begin == span.begin && end == span.end;
        }
    };
} // namespace cial::Common
