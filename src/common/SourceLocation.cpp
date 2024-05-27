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

#include "SourceLocation.hpp"

using namespace Ciallang::Common;

const LocationType& SourceLocation::end() const {
    return _end;
}

const LocationType& SourceLocation::start() const {
    return _start;
}

void SourceLocation::end(const LocationType& value) {
    _end = value;
}

void SourceLocation::start(const LocationType& value) {
    _start = value;
}

void SourceLocation::end(const uint32_t line, const uint32_t column) {
    _end.line = line;
    _end.column = column;
}

void SourceLocation::start(const uint32_t line, const uint32_t column) {
    _start.line = line;
    _start.column = column;
}
