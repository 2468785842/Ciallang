#pragma once

#include <string>
#include <string_view>

namespace cial {
    struct TextStorage {
        std::string sourceName;
        std::string text; // 解码后的 UTF-8 源码，进入前端时仍执行严格校验。
    };

    class TextStorageReader {
    public:
        virtual ~TextStorageReader() = default;
        // 保留虚拟路径和 mode 原文；由宿主接 XP3/TVP 流及其编码规则。
        // from 是调用块的源码身份；失败抛 std::exception，脚本可以 catch。
        // reader 不自动追踪脚本引用；宿主闭包中的 Value 仍需 Handle 保根。
        virtual TextStorage read(std::string_view name, std::string_view mode, std::string_view from) = 0;
    };
} // namespace cial
