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

#include "SourceFile.hpp"

#include <fstream>

#include <fmt/format.h>

#include "Colorizer.hpp"
#include "Defer.hpp"
#include "UTF8.hpp"

namespace Ciallang::Common {

    void SourceFile::error(
            Result &r,
            const std::string &message,
            const SourceLocation &location) const {
        std::stringstream stream{""};

        const auto numberOfLines = static_cast<int32_t>(_linesByNumber.size());
        const auto targetLine = static_cast<int32_t>(location.start().line);
        const auto messageIndicator = Colorizer::colorize(
                "^ " + message,
                fmt::color::red);

        auto startLine = static_cast<int32_t>(location.start().line - 1);
        if (startLine < 0)
            startLine = 0;

        auto stopLine = static_cast<int32_t>(location.end().line + 1);
        if (stopLine >= numberOfLines)
            stopLine = numberOfLines;

        for (int32_t i = startLine; i < stopLine; i++) {
            const auto sourceLine = lineByNumber(static_cast<size_t>(i));
            if (sourceLine == nullptr)
                break;
            const auto sourceText = substring(
                    sourceLine->begin,
                    sourceLine->end);
            if (i == targetLine) {
                stream << Colorizer::colorize(
                        fmt::format("{:>4d}| ", i + 1),
                        fmt::color::violet)
                       << Colorizer::colorizeRange(
                               sourceText,
                               location.start().column,
                               location.end().column,
                               fmt::color::yellow,
                               fmt::color::blue)
                       << '\n'
                       << std::string(6 + location.start().column, ' ')
                       << messageIndicator;
            } else {
                stream << Colorizer::colorize(
                        fmt::format("{:>4d}| ", i + 1),
                        fmt::color::violet)
                       << sourceText;
            }

            if (i < stopLine - 1)
                stream << "\n";
        }

        r.error(
                fmt::format(
                        "({}@{}:{}) {}",
                        _path.filename().string(),
                        location.start().line + 1,
                        location.start().column + 1,
                        message),
                location,
                stream.str());
    }

    void SourceFile::pushMark() {
        _markStack.push(_index);
    }

    bool SourceFile::eof() const {
        return _index > _buffer.size() - 1;
    }

    size_t SourceFile::popMark() {
        if (_markStack.empty())
            return _index;
        const auto mark = _markStack.top();
        _markStack.pop();
        return mark;
    }

    size_t SourceFile::pos() const {
        return _index;
    }

    bool SourceFile::empty() const {
        return _buffer.empty();
    }

    void SourceFile::buildLines(Result &r) {
        uint32_t line = 0;
        uint32_t columns = 0;
        size_t lineStart = 0;

        while (true) {
            auto rune = next(r);

            if (rune == runeInvalid) break;
            if (rune == runeBom) rune = next(r);

            const auto endOfBuffer = rune == runeEof;
            const auto unixNewLine = rune == '\n';

            if (unixNewLine || endOfBuffer) {
                const auto end = endOfBuffer ? _buffer.size() : _index - 1;
                const auto it = _linesByIndexRange.insert(std::make_pair(
                        std::make_pair(lineStart, end),
                        SourceFileLineType{
                                .end = end,
                                .begin = lineStart,
                                .line = line,
                                .columns = columns
                        }));
                _linesByNumber.insert(std::make_pair(
                        line,
                        &it.first->second));
                lineStart = _index;
                line++;
                columns = 0;
            } else {
                columns++;
            }

            if (rune == runeEof)
                break;
        }

        seek(0);
    }

    size_t SourceFile::currentMark() {
        if (_markStack.empty())
            return _index;
        return _markStack.top();
    }

    size_t SourceFile::length() const {
        return _buffer.size();
    }

    void SourceFile::restoreTopMark() {
        if (_markStack.empty()) return;
        _index = _markStack.top();
    }

    void SourceFile::seek(const size_t index) {
        _index = index;
    }

    bool SourceFile::load(Result &r, const std::string &buffer) {
        _buffer.clear();
        _linesByNumber.clear();
        _linesByIndexRange.clear();

        std::stringstream stream;
        stream.unsetf(std::ios::skipws);
        stream << buffer;
        stream.seekg(0, std::ios::beg);

        _buffer.reserve(buffer.length());
        _buffer.insert(_buffer.begin(),
                       std::istream_iterator<uint8_t>(stream),
                       std::istream_iterator<uint8_t>());
        buildLines(r);

        return true;
    }

    void SourceFile::dumpLines() const {
        for (size_t i = 0; i < numberOfLines(); i++) {
            const auto line = lineByNumber(i);
            fmt::print("{}\n", substring(line->begin, line->end));
        }
    }

    bool SourceFile::load(Result &r) {
        _buffer.clear();
        _linesByNumber.clear();
        _linesByIndexRange.clear();

        if (ifstream file{
                    _path.string(),
                    std::ios::in | std::ios::binary
            }; file.is_open()) {

            file.unsetf(std::ios::skipws);
            file.seekg(0, std::ios::end);
            const auto file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            _buffer.reserve(file_size);
            _buffer.insert(_buffer.begin(),
                           std::istream_iterator<uint8_t>(file),
                           std::istream_iterator<uint8_t>());
            buildLines(r);
        } else {
            r.error(
                    fmt::format("unable to open source file: {}", _path.string()));
        }
        return !r.isFailed();
    }

    size_t SourceFile::numberOfLines() const {
        return _linesByNumber.size();
    }

    int32_t SourceFile::next(Result &r) {
        size_t width = 1;

        DEFER { _index += width; };

        if (_buffer.empty() || _index > _buffer.size() - 1)
            return runeEof;

        const auto ch = _buffer[_index];
        int32_t rune = ch;
        if (ch == 0) {
            r.error("illegal character NUL");
            return runeInvalid;
        }

        if (ch >= 0x80) {
            const auto cp = utf8Decode(
                    reinterpret_cast<char *>(_buffer.data() + _index),
                    _buffer.size() - _index);

            width = cp.width;
            rune = cp.value;

            if (rune == runeInvalid && width == 1) {
                r.error("illegal utf-8 encoding");
                return runeInvalid;
            }

            if (rune == runeBom && _index > 0) {
                r.error("illegal byte order mark");
                return runeInvalid;
            }

        } else if (ch == '\r' && _buffer.size() != _index + 1) {
            // Windows line feed
            if (_buffer[_index + 1] == '\n') {
                _index++;
                return '\n';
            }
        }

        return rune;
    }

    uint8_t SourceFile::operator[](const size_t index) const {
        return _buffer[index];
    }

    const filesystem::path &SourceFile::path() const {
        return _path;
    }

    uint32_t SourceFile::columnByIndex(const size_t index) const {
        const auto line = lineByIndex(index);
        if (line == nullptr) return 0;
        return static_cast<const uint32_t>(index - line->begin);
    }

    std::string SourceFile::substring(const size_t start, const size_t end) const {
        const auto length = end - start;
        std::string value;
        value.reserve(length);
        value.assign(reinterpret_cast<const char *>(_buffer.data()), start, length);
        return value;
    }

    const SourceFileLineType *SourceFile::lineByNumber(const size_t line) const {
        const auto it = _linesByNumber.find(line);
        if (it == _linesByNumber.end()) return nullptr;
        return it->second;
    }

    const SourceFileLineType *SourceFile::lineByIndex(size_t index) const {
        const auto it = _linesByIndexRange.find(
                std::make_pair(index, index)
        );
        // yes, we need, if not found return last iterator
        if (it == _linesByIndexRange.end()) return nullptr;
        return &it->second;
    }
}