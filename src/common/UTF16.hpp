#pragma once

#include <string>
#include <string_view>

namespace cial::Common {
    // 脚本位置按 UTF-16 码元计数；内部字节串用 WTF-8 保留 charAt/substring
    // 得到的孤立代理码元。源文件仍由 SourceFile 严格校验为合法 UTF-8。
    std::u16string toUtf16(std::string_view text);
    std::string fromUtf16(std::u16string_view text);
    std::string fromUtf16LE(std::string_view bytes);
    std::string toUtf16LE(std::string_view text);
    size_t utf16Length(std::string_view text);
    std::string utf16Slice(std::string_view text, size_t start, size_t count);
} // namespace cial::Common
