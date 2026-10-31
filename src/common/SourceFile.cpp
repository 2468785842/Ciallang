/*
 * Copyright (c) 2024/5/6 下午8:16
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

#include "SourceFile.hpp"


#include "Colorizer.hpp"
#include "Defer.hpp"
#include "UTF8.hpp"

namespace cial::Common {
    namespace {
        // 摘录展示的上下文行数（目标行前后各 N 行）
        constexpr u32 ExcerptContextLines = 1;

        // 摘录行号栏宽；行号右对齐，栏固定 5 列（"NNNN | "）
        constexpr u32 ExcerptGutterWidth = 4;

        // 源码行内制表符按 4 列展开，指示符同步对齐
        constexpr u32 TabDisplayWidth = 4;

        // 单行摘录内高亮的上限，避免超长范围拖垮输出
        constexpr size_t ExcerptMaxHighlight = 120;

        // 摘录中源码行的最大字节数（超出截断加省略号）
        constexpr size_t ExcerptMaxLineBytes = 400;
    } // namespace

    std::string SourceFile::sourceName() const {
        if(!_sourceName.empty())
            return _sourceName;
        if(!_path.empty())
            return _path.string();
        return "<input>";
    }

    std::string SourceFile::substring(const size_t start, const size_t end) const {
        if(start > end || end > _buffer.size())
            throw std::out_of_range("source substring range");
        if(start == end)
            return {};
        return { reinterpret_cast<const char*>(_buffer.data() + start), end - start };
    }

    // 摘录渲染：行号栏 + 目标行 + 按显示宽度对齐的指示符。
    // 多行范围截断到首行并提示续行；零宽范围为插入点 '^'。
    std::string SourceFile::renderExcerpt(const SourceSpan& span) const {
        if(!span.valid())
            return {};

        const SourceLocation location = locationBySpan(span);
        const auto numberOfLines = static_cast<i32>(this->numberOfLines());
        auto startLine = static_cast<i32>(location.start().line) - static_cast<i32>(ExcerptContextLines);
        if(startLine < 0)
            startLine = 0;
        auto stopLine = static_cast<i32>(location.end().line) + static_cast<i32>(ExcerptContextLines) + 1;
        if(stopLine > numberOfLines)
            stopLine = numberOfLines;

        std::stringstream stream;
        const auto targetLine = static_cast<i32>(location.start().line);

        for(i32 i = startLine; i < stopLine; i++) {
            const auto* sourceLine = lineByNumber(static_cast<size_t>(i));
            if(sourceLine == nullptr)
                break;

            auto lineText = substring(sourceLine->begin, sourceLine->end);
            const bool truncated = lineText.length() > ExcerptMaxLineBytes;
            if(truncated)
                lineText = lineText.substr(0, ExcerptMaxLineBytes) + "...";

            // 制表符按固定宽度展开，并保留原始字节偏移 -> 展开后偏移的映射
            std::vector<size_t> byteMap;
            if(lineText.find('\t') != std::string::npos) {
                std::string expanded;
                byteMap.reserve(lineText.length() + 1);
                for(const char ch : lineText) {
                    byteMap.push_back(expanded.length());
                    expanded.append(ch == '\t' ? std::string(TabDisplayWidth, ' ') : std::string(1, ch));
                }
                byteMap.push_back(expanded.length());
                lineText = std::move(expanded);
            }

            stream << Colorizer::colorize(
                fmt::format("{:>{}} | ", i + 1, ExcerptGutterWidth), fmt::color::violet);

            if(i != targetLine) {
                stream << lineText << '\n';
                continue;
            }

            // 目标行内的行相对字节范围（多行范围截断到本行末尾）
            const size_t beginRel = std::clamp(span.begin, sourceLine->begin, sourceLine->end) - sourceLine->begin;
            const size_t endRel = std::clamp(span.end, span.begin, sourceLine->end) - sourceLine->begin;
            const auto toExpanded = [&byteMap](const size_t byteOffset) {
                return byteMap.empty() ? byteOffset : byteMap[std::min(byteOffset, byteMap.size() - 1)];
            };
            const size_t beginByte = toExpanded(beginRel);
            const size_t endByte = std::max(toExpanded(endRel), beginByte);

            const size_t beginWidth = utf8DisplayWidth(lineText.data(), beginByte);
            const size_t markWidth = utf8DisplayWidth(lineText.data() + beginByte, endByte - beginByte);

            std::string marks;
            if(endByte > beginByte)
                marks.assign(std::min(std::max(markWidth, size_t{ 1 }), ExcerptMaxHighlight), '^');
            else
                marks = "^"; // 零宽：插入点

            stream << Colorizer::colorizeRange(lineText, beginByte, endByte, fmt::color::yellow, fmt::color::blue)
                    << '\n'
                    // 行号栏 "{:>4} | " 共 ExcerptGutterWidth+3 列，指示符按显示宽度对齐
                    << std::string(ExcerptGutterWidth + 3, ' ') << std::string(beginWidth, ' ')
                    << Colorizer::colorize(marks, fmt::color::red);

            if(location.end().line > location.start().line)
                stream << fmt::format(" (continues to line {})", location.end().line + 1);
            if(truncated)
                stream << " (line truncated)";
            stream << '\n';
        }

        return stream.str();
    }

    void SourceFile::error(Result& r, const std::string& message, const SourceLocation& location,
                           const std::string& code) const {
        // 行列入口兼容旧调用（IRGenerator 等）；消息保持纯文本，前缀由展示层拼接
        r.error(message, location, {}, code, sourceName(), {}, true);
    }

    void SourceFile::error(Result& r, const std::string& message, const SourceSpan& span,
                           const std::string& code) const {
        r.error(message, locationBySpan(span), renderExcerpt(span), code, sourceName(), span, true);
    }

    SourceLocation SourceFile::locationByIndex(const size_t index) const {
        SourceLocation location;
        if(_linesByNumber.empty()) {
            location.start(0, 0);
            location.end(0, 0);
            return location;
        }

        const auto clamped = std::min(index, _buffer.size());
        const auto* line = lineByIndex(clamped);
        if(line == nullptr) {
            // 行表未覆盖（不应发生）：收敛到末行
            const auto* last = _linesByNumber.rbegin()->second;
            location.start(last->line, static_cast<u32>(last->end - last->begin));
            location.end(last->line, static_cast<u32>(last->end - last->begin));
            return location;
        }

        const auto column = static_cast<u32>(clamped - line->begin);
        location.start(line->line, column);
        location.end(line->line, column);
        return location;
    }

    SourceLocation SourceFile::locationBySpan(const SourceSpan& span) const {
        if(!span.valid())
            return {};
        const auto begin = locationByIndex(span.begin);
        const auto end = locationByIndex(span.end);
        SourceLocation location;
        location.start(begin.start().line, begin.start().column);
        location.end(end.end().line, end.end().column);
        return location;
    }

    SourceSpan SourceFile::spanOf(const size_t begin, const size_t end) const {
        if(begin > end || end > _buffer.size())
            return {};
        return { begin, end };
    }

    std::string SourceFile::renderExcerptAt(const u32 line, const u32 column) const {
        const auto* lineEntry = lineByNumber(line);
        if(lineEntry == nullptr)
            return {};
        // 行列 -> 行内字节偏移（列越界收敛到行尾，零宽插入点）
        const size_t width = lineEntry->end - lineEntry->begin;
        const size_t offset = std::min<size_t>(column, width);
        const size_t begin = lineEntry->begin + offset;
        const size_t end = offset < width ? begin + 1 : begin;
        return renderExcerpt(spanOf(begin, end));
    }

    void SourceFile::pushMark() { _markStack.push(_index); }

    bool SourceFile::eof() const { return _index >= _buffer.size(); }

    size_t SourceFile::popMark() {
        if(_markStack.empty())
            return _index;
        const auto mark = _markStack.top();
        _markStack.pop();
        return mark;
    }

    size_t SourceFile::pos() const { return _index; }

    bool SourceFile::empty() const { return _buffer.empty(); }

    void SourceFile::buildLines() {
        _linesByNumber.clear();
        _linesByIndexRange.clear();

        u32 line = 0;
        size_t lineStart = 0;
        const size_t size = _buffer.size();

        auto addLine = [this](const size_t begin, const size_t end, const u32 number) {
            const auto [fst, snd] = _linesByIndexRange.insert(std::make_pair(
                std::make_pair(begin, end),
                SourceFileLineType{ .end = end, .begin = begin, .line = number, .columns = 0 }));
            _linesByNumber.insert(std::make_pair(number, &fst->second));
        };

        for(size_t i = 0; i < size; ++i) {
            if(_buffer[i] != '\n')
                continue;
            auto end = i;
            if(end > lineStart && _buffer[end - 1] == '\r')
                --end; // CRLF：\r 不计入行内容
            addLine(lineStart, end, line);
            ++line;
            lineStart = i + 1;
        }

        // 无条件保留末行：无换行符的末行、以换行结尾的空行、空文件均有定义；
        // EOF 偏移（== size）总能定位到末行
        addLine(lineStart, size, line);

        seek(0);
    }

    // 装载校验：非法 UTF-8、NUL 与文件体内 BOM 在此带字节位置上报；
    // 行表由纯字节扫描构建，校验失败也不影响后续按字节定位
    void SourceFile::validateBytes(Result& r) const {
        const size_t size = _buffer.size();
        size_t i = 0;

        if(size >= 3 && _buffer[0] == 0xEF && _buffer[1] == 0xBB && _buffer[2] == 0xBF)
            i = 3; // 文件头 BOM 合法

        while(i < size) {
            if(_buffer[i] == 0) {
                r.error("illegal character NUL", locationByIndex(i), {}, "lex.invalid-byte", sourceName(),
                    spanOf(i, i + 1), true);
                return;
            }
            if(_buffer[i] < 0x80) {
                ++i;
                continue;
            }
            const auto cp = utf8Decode(reinterpret_cast<const char*>(_buffer.data() + i), size - i);
            if(cp.value == runeInvalid || cp.width <= 0) {
                r.error("illegal utf-8 encoding", locationByIndex(i), {}, "lex.invalid-byte", sourceName(),
                    spanOf(i, i + 1), true);
                return;
            }
            if(cp.value == runeBom) {
                r.error("illegal byte order mark", locationByIndex(i), {}, "lex.invalid-byte", sourceName(),
                    spanOf(i, i + cp.width), true);
                return;
            }
            i += static_cast<size_t>(cp.width);
        }
    }

    size_t SourceFile::currentMark() {
        if(_markStack.empty())
            return _index;
        return _markStack.top();
    }

    size_t SourceFile::length() const { return _buffer.size(); }

    void SourceFile::restoreTopMark() {
        if(_markStack.empty())
            return;
        _index = _markStack.top();
    }

    void SourceFile::seek(const size_t index) { _index = index; }

    bool SourceFile::load(Result& r, const std::string& buffer) {
        _index = 0;
        _lastWidth = 0;
        _markStack = {};
        _buffer.clear();
        _linesByNumber.clear();
        _linesByIndexRange.clear();

        _buffer.reserve(buffer.length());
        _buffer.assign(buffer.begin(), buffer.end());
        buildLines();
        validateBytes(r);

        return !r.isFailed();
    }

    void SourceFile::dumpLines() const {
        for(size_t i = 0; i < numberOfLines(); i++) {
            const auto line = lineByNumber(i);
            fmt::print("{}\n", substring(line->begin, line->end));
        }
    }

    bool SourceFile::load(Result& r) {
        _index = 0;
        _lastWidth = 0;
        _markStack = {};
        _buffer.clear();
        _linesByNumber.clear();
        _linesByIndexRange.clear();

        if(std::ifstream file{ _path.string(), std::ios::in | std::ios::binary }; file.is_open()) {
            file.unsetf(std::ios::skipws);
            file.seekg(0, std::ios::end);
            const auto file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            _buffer.reserve(file_size);
            _buffer.insert(_buffer.begin(), std::istream_iterator<u8>(file), std::istream_iterator<u8>());
            buildLines();
            validateBytes(r);
        } else {
            // 读取失败无源码位置：不伪造 (0,0)
            r.error(fmt::format("unable to open source file: {}", _path.string()), {}, {}, "io.read");
        }
        return !r.isFailed();
    }

    size_t SourceFile::numberOfLines() const { return _linesByNumber.size(); }

    i32 SourceFile::next(Result& r) {
        size_t width = 1;

        DEFER {
            _index += width;
            _lastWidth = width;
        };

        if(_buffer.empty() || _index >= _buffer.size()) {
            // EOF 时不能推进 _index，否则 pos 会越过缓冲区末尾，
            // 后续按下标回溯（rewindOneChar）将越界访问
            width = 0;
            return runeEof;
        }

        const auto ch = _buffer[_index];
        i32 rune = ch;
        if(ch == 0) {
            r.error("illegal character NUL");
            return runeInvalid;
        }

        if(ch >= 0x80) {
            const auto cp = utf8Decode(reinterpret_cast<char*>(_buffer.data() + _index), _buffer.size() - _index);

            width = cp.width;
            rune = cp.value;

            if(rune == runeInvalid) {
                r.error("illegal utf-8 encoding");
                return runeInvalid;
            }

            if(rune == runeBom && _index > 0) {
                r.error("illegal byte order mark");
                return runeInvalid;
            }
        } else if(ch == '\r' && _buffer.size() != _index + 1) {
            // Windows line feed
            if(_buffer[_index + 1] == '\n') {
                _index++;
                return '\n';
            }
        }

        return rune;
    }

    u8 SourceFile::operator[](const size_t index) const { return _buffer[index]; }

    const std::filesystem::path& SourceFile::path() const { return _path; }

    const SourceFileLineType* SourceFile::lineByNumber(const size_t line) const {
        const auto it = _linesByNumber.find(line);
        if(it == _linesByNumber.end())
            return nullptr;
        return it->second;
    }

    const SourceFileLineType* SourceFile::lineByIndex(size_t index) const {
        const auto it = _linesByIndexRange.find(std::make_pair(index, index));
        if(it == _linesByIndexRange.end())
            return nullptr;
        return &it->second;
    }

    u32 SourceFile::columnByIndex(const size_t index) const {
        const auto line = lineByIndex(index);
        if(line == nullptr)
            return 0;
        return static_cast<const u32>(index - line->begin);
    }
} // namespace cial::Common
