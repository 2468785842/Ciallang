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

#include <string>
#include <vector>

#include "SourceLocation.hpp"
#include "SourceSpan.hpp"

namespace cial::Common {
    // 结构化诊断消息：核心产生纯文本 message 与稳定 code，
    // 位置（span/行列）、源码身份与摘录随消息携带；终端渲染、JSON
    // 序列化与 LSP 适配各自消费，不解析 message 文本判断错误类型。
    class ResultMessage {
    public:
        enum Types { info, error, warning, data };

        explicit ResultMessage(std::string message, const SourceLocation loc = {}, std::string details = {},
                               const Types type = info, std::string code = {}, std::string source = {},
                               const SourceSpan span = {}, const bool located = false) :
            _type(type),
            _message(std::move(message)), _details(std::move(details)), _location(loc), _code(std::move(code)),
            _source(std::move(source)), _span(span), _located(located) {
        }

        [[nodiscard]] Types type() const noexcept { return _type; }

        [[nodiscard]] bool isError() const noexcept { return _type == error; }

        [[nodiscard]] const std::string& details() const noexcept { return _details; }

        [[nodiscard]] const std::string& message() const noexcept { return _message; }

        [[nodiscard]] const SourceLocation& location() const noexcept { return _location; }

        // 稳定错误码（如 "parse.expected-token"）；未迁移路径为空
        [[nodiscard]] const std::string& code() const noexcept { return _code; }

        // 源码身份（路径或逻辑名）；无源码上下文为空
        [[nodiscard]] const std::string& source() const noexcept { return _source; }

        // 权威字节范围（半开）；无位置错误（文件读取/宿主故障）无 span
        [[nodiscard]] const SourceSpan& span() const noexcept { return _span; }

        [[nodiscard]] bool hasSpan() const noexcept { return _span.valid(); }

        // 是否携带有效位置；未定位消息不得用 (0,0) 伪造
        [[nodiscard]] bool hasLocation() const noexcept { return _located; }

    private:
        Types _type;
        std::string _message{};
        std::string _details{};
        SourceLocation _location{};
        std::string _code{};
        std::string _source{};
        SourceSpan _span{};
        bool _located = false;
    };

    using ResultMessageList = std::vector<ResultMessage>;
} // namespace cial::Common
