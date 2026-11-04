#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace cial::format {
    // 读取整个文件（二进制）；失败抛 std::runtime_error。
    std::string readFile(const std::filesystem::path &path);

    // 安全写回：拒绝符号链接；original 与当前内容不符时拒绝，避免覆盖并发修改。
    // 同目录排他创建临时文件，完整写入并保留权限后原子替换，失败清理临时文件。
    void replaceFile(const std::filesystem::path &path, std::string_view original, std::string_view formatted);
} // namespace cial::format
