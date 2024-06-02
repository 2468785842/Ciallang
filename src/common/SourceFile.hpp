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

#include "Result.hpp"

namespace Ciallang::Common {
    using namespace std;
    using SourceFileRangeType = pair<size_t, size_t>;

    struct SourceFileRangeCompareType {
        bool operator()(
                const SourceFileRangeType &lhs,
                const SourceFileRangeType &rhs) const {
            return lhs.second < rhs.first;
        }
    };

    struct SourceFileLineType {
        size_t end = 0;
        size_t begin = 0;
        uint32_t line = 0;
        uint32_t columns = 0;
    };

    class SourceFile {
    public:
        SourceFile() = default;

        explicit SourceFile(filesystem::path path) : _path(std::move(path)) {
        }

        ~SourceFile() = default;

        bool load(
                Result &r,
                const std::string &buffer);

        void error(
                Result &r,
                const string &message,
                const SourceLocation &location) const;

        [[nodiscard]] bool eof() const;

        void pushMark();

        size_t popMark();

        [[nodiscard]] size_t pos() const;

        [[nodiscard]] bool empty() const;

        [[nodiscard]] size_t length() const;

        size_t currentMark();

        void seek(size_t index);

        void restoreTopMark();

        bool load(Result &r);

        int32_t next(Result &r);

        [[nodiscard]] size_t numberOfLines() const;

        uint8_t operator[](size_t index) const;

        [[nodiscard]] const filesystem::path &path() const;

        [[nodiscard]] uint32_t columnByIndex(size_t index) const;

        [[nodiscard]] string substring(size_t start, size_t end) const;

        [[nodiscard]] const SourceFileLineType *lineByNumber(size_t line) const;

        [[nodiscard]] const SourceFileLineType *lineByIndex(size_t index) const;

    private:
        void dumpLines() const;

        void buildLines(Result &r);

        size_t _index = 0;
        filesystem::path _path;
        vector<uint8_t> _buffer;
        stack<size_t> _markStack{};
        map<size_t, SourceFileLineType *> _linesByNumber{};
        map<
                SourceFileRangeType,
                SourceFileLineType,
                SourceFileRangeCompareType> _linesByIndexRange{};
    };
}
