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

#include "pch.h"

namespace Ciallang::Common {

    struct LocationType {
        uint32_t line = 0;
        uint32_t column = 0;

        bool operator==(const LocationType& type) const {
            return line == type.line && column == type.column;
        }
    };

    class SourceLocation {
        LocationType _end;
        LocationType _start;

    public:
        [[nodiscard]] const LocationType& end() const;

        [[nodiscard]] const LocationType& start() const;

        void end(const LocationType& value);

        void start(const LocationType& value);

        void end(uint32_t line, uint32_t column);

        void start(uint32_t line, uint32_t column);

        bool operator==(const SourceLocation& sourceLocation) const {
            return _start == sourceLocation._start && _end == sourceLocation._end;
        }
    };
}
