/*
 * Copyright (c) 2024/6/24 下午6:02
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

#include "pch.h"

namespace Ciallang::Common {
    class TableFormatter {
        template <uint8_t L1>
        uint8_t mergeX(const std::any (&table)[L1],
                       const uint8_t curX
        ) {
            uint8_t size{ 1 };
            for(uint8_t i = curX + 1; i < L1; ++i) {
                if(!isEqual(table[curX], table[i])) {
                    break;
                }
                ++size;
            }
            return size;
        }

        template <uint8_t L1, uint8_t L2>
        std::any mergeY(const std::any (&table)[L1][L2],
                        const uint8_t curX,
                        const uint8_t curY
        ) {
            if(curY == L1 - 1 || curX == 0) {
                return table[curY][curX];
            }

            uint8_t absIndex{ 0 };
            uint8_t continuousSize{ 0 };
            auto upCmp = table[curY][curX];
            auto downCmp = table[curY][curX];

            if(isRowEmpty(table[curY])) {
                upCmp = table[curY + 1][curX];
                downCmp = table[curY - 1][curX];
            }

            for(uint8_t i = curY; i > 0; --i) {
                if(isRowEmpty(table[i - 1])) continue;

                if(isEqual(upCmp, table[i - 1][curX])) {
                    ++absIndex;
                    ++continuousSize;
                }
            }

            for(uint8_t i = curY + 1; i < L1; ++i) {
                if(isRowEmpty(table[i])) continue;

                if(isEqual(downCmp, table[i][curX])) {
                    ++continuousSize;
                }
            }

            if(absIndex == 0 && continuousSize == 0) {
                return table[curY][curX];
            }

            if(continuousSize >> 1 == absIndex && isRowEmpty(table[curY])) {
                return table[curY - 1][curX];
            }

            return "";
        }

        template <uint8_t L1, uint8_t L2>
        std::string createDivider(const std::any (&table)[L1][L2], const uint8_t y) {
            std::stringstream ss{};
            if(isRowEmpty(table[y - 1])) {
                throw std::invalid_argument("bad arguments");
            }
            for(uint8_t i = 0; i < L2;) {
                uint8_t size = mergeX(table[y - 1], i);
                auto temp = mergeY(table, i, y);
                if(temp.has_value()) {
                    ss << fmt::format("+{: ^{}}", anyToString(temp), cellSize() * size + size - 1);
                } else {
                    ss << fmt::format("+{:-^{}}", "", cellSize() * size + size - 1);
                }

                i += size;
            }
            ss << '+';

            return ss.str();
        }

        template <uint8_t L1, uint8_t L2>
        std::string createCell(const std::any (&table)[L1][L2], const uint8_t y) {
            std::stringstream ss{};
            char closeChar = '+';
            for(uint8_t i = 0; i < L2; ++i) {
                uint8_t size = mergeX(table[y], i);
                auto temp = mergeY(table, i, y);
                if(temp.has_value()) {
                    ss << fmt::format("|{: ^{}}", anyToString(temp), cellSize() * size + size - 1);
                    closeChar = '|';
                } else {
                    ss << fmt::format("+{:-^{}}", "", cellSize() * size + size - 1);
                    closeChar = '+';
                }

                i += size - 1;
            }

            ss << closeChar;

            return ss.str();
        }

    public:
        TableFormatter(const size_t tableSize, const uint8_t columnCount)
            : _tableSize(tableSize), _columnCount(columnCount) {
        }

        template <uint8_t L1, uint8_t L2>
        void parse(const std::any (&table)[L1][L2]) {
            std::stringstream ss{};

            _buffer.push_back(fmt::format("+{:-^{}}+\n", "", tableSize()));

            for(uint8_t i = L1; i > 0; --i) {
                if(isRowEmpty(table[i - 1]) && i - 1 > 0) {
                    ss << createDivider(table, i - 1);
                } else {
                    ss << createCell(table, i - 1);
                }
                ss << '\n';
                _buffer.push_back(ss.str());

                ss.str("");
            }

            _buffer.push_back(fmt::format("+{:-^{}}+\n", "", tableSize()));
        }

        std::string format() const {
            std::ostringstream ss;
            for(const auto& line : _buffer | std::views::reverse) {
                ss << line;
            }
            return ss.str();
        }

        void clearBuf() { _buffer.clear(); }

        void println() {
            fmt::println("{}", format());
            clearBuf();
        }

    private:
        size_t _tableSize;
        uint8_t _columnCount;
        std::vector<std::string> _buffer{};

        static bool isRowEmpty(const std::any* row) {
            return !row->has_value();
        }

        size_t cellSize() const {
            return _tableSize / _columnCount;
        }

        size_t tableSize() const {
            return _tableSize + (_columnCount - 1);
        }

        bool isEqual(const std::any& a, const std::any& b) const {
            if(a.type() != b.type())
                return false;

            if(a.type() == typeid(std::string)
               || a.type() == typeid(const char*)
            )
                return anyToString(a) == anyToString(b);

            return false;
        }

        std::string anyToString(const std::any& value) const {
            if(value.type() == typeid(const char*))
                return std::any_cast<const char*>(value);
            if(value.type() == typeid(std::string))
                return std::any_cast<std::string>(value);
            if(value.type() == typeid(double))
                return fmt::format("{:.2f}", std::any_cast<double>(value));
            if(value.type() == typeid(int))
                return fmt::format("{}", std::any_cast<int>(value));
            if(value.type() == typeid(size_t))
                return fmt::format("{}", std::any_cast<size_t>(value));

            throw std::runtime_error("Unsupported type");
        }
    };
}
