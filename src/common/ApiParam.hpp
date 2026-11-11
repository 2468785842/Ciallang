#pragma once

#include <string>

namespace cial {
    // 注册元数据与声明解析共用；默认值为声明字面量，绝不求值。
    struct ApiParam {
        std::string name;
        std::string type;
        bool optional{};
        bool collapse{};
        std::string defaultText;
    };
} // namespace cial
