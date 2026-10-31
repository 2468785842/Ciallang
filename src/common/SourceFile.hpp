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

#include <filesystem>
#include <map>
#include <stack>
#include <vector>
#include "Result.hpp"
#include "SourceSpan.hpp"

namespace cial::Common {
    using SourceFileRangeType = std::pair<size_t, size_t>;

    struct SourceFileRangeCompareType {
        bool operator()(const SourceFileRangeType &lhs, const SourceFileRangeType &rhs) const {
            return lhs.second < rhs.first;
        }
    };

    struct SourceFileLineType {
        size_t end = 0;
        size_t begin = 0;
        u32 line = 0;
        u32 columns = 0;
    };

    class SourceFile {
    public:
        SourceFile() = default;

        explicit SourceFile(std::filesystem::path path) : _path(std::move(path)) {}

        ~SourceFile() = default;

        // 纯文本诊断消息 + 已知位置：摘录与位置信息由消息携带，文本前缀由展示层拼接
        void error(Result &r, const std::string &message, const SourceLocation &location,
                   const std::string &code = {}) const;

        // 字节范围版本：span 为权威位置，行列按行索引换算后一并存入消息
        void error(Result &r, const std::string &message, const SourceSpan &span, const std::string &code = {}) const;

        // 无位置错误（文件读取失败等）直接走 Result::error，不伪造 (0,0) 位置

        // 源码身份：默认为路径字符串；REPL/eval/宿主可设置逻辑名
        [[nodiscard]] std::string sourceName() const;

        void setSourceName(std::string name) { _sourceName = std::move(name); }

        // 字节偏移 -> 行列（0 基，列为字节列）；越界/空文件安全（收敛到首末行）
        [[nodiscard]] SourceLocation locationByIndex(size_t index) const;

        [[nodiscard]] SourceLocation locationBySpan(const SourceSpan &span) const;

        // 构造并夹紧到 [0, length] 的半开区间；非法输入返回未知 span
        [[nodiscard]] SourceSpan spanOf(size_t begin, size_t end) const;

        // 按行列（0 基，字节列）渲染摘录：供运行期故障等只有行列、
        // 无字节 span 的场景；行号越界返回空串
        [[nodiscard]] std::string renderExcerptAt(u32 line, u32 column) const;

        // EOF 插入点 [length, length)
        [[nodiscard]] SourceSpan eofSpan() const { return SourceSpan{ _buffer.size(), _buffer.size() }; }

        [[nodiscard]] bool eof() const;

        void pushMark();

        size_t popMark();

        [[nodiscard]] size_t pos() const;

        // 最近一次 next() 实际消耗的字节数；EOF 时为 0，
        // 供回溯判断是否有字符可退
        [[nodiscard]] size_t lastWidth() const { return _lastWidth; }

        [[nodiscard]] bool empty() const;

        [[nodiscard]] size_t length() const;

        size_t currentMark();

        void seek(size_t index);

        void restoreTopMark();

        bool load(Result &r);

        bool load(Result &r, const std::string &buffer);

        i32 next(Result &r);

        [[nodiscard]] size_t numberOfLines() const;

        u8 operator[](size_t index) const;

        [[nodiscard]] const std::filesystem::path &path() const;

        [[nodiscard]] u32 columnByIndex(size_t index) const;

        [[nodiscard]] std::string substring(size_t start, size_t end) const;

        [[nodiscard]] const SourceFileLineType *lineByNumber(size_t line) const;

        [[nodiscard]] const SourceFileLineType *lineByIndex(size_t index) const;

    private:
        void dumpLines() const;

        // 摘录渲染：行号栏 + 目标行 + 显示宽度对齐的指示符（多行截断到首行）
        [[nodiscard]] std::string renderExcerpt(const SourceSpan &span) const;

        // 纯字节扫描建行表：与 UTF-8 有效性解耦，非法字节也能给出有界行位置；
        // LF/CRLF/空文件/无末尾换行/BOM 均有定义，空文件保留一个空行
        void buildLines();

        // 装载后字节级校验：非法 UTF-8/NUL/文件体内 BOM 带字节位置上报
        void validateBytes(Result &r) const;

        size_t _index = 0;
        size_t _lastWidth = 0;
        std::filesystem::path _path;
        std::string _sourceName{};
        std::vector<u8> _buffer;
        std::stack<size_t> _markStack{};
        std::map<size_t, SourceFileLineType *> _linesByNumber{};
        std::map<SourceFileRangeType, SourceFileLineType, SourceFileRangeCompareType> _linesByIndexRange{};
    };
} // namespace cial::Common