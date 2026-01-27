// Copyright (c) 2026/1/5 16:33
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//
#pragma once

#include "types/String.hpp"
#include "types/Value.hpp"

namespace cial {
    class VM;
}

namespace cial::stdlib {

    String stringCharAt(VM *, const Value &thisObj, size_t argCount, Integer index);

    Integer stringIndexOf(VM *, const Value &thisObj, size_t argCount, const String *subStr, const Value &vStart);

    void stringToUpperCase(VM *, const Value &thisObj, size_t argCount);

    void stringToLowerCase(VM *, const Value &thisObj, size_t argCount);

    String stringSubstring(VM *, const Value &thisObj, size_t argCount, Integer start, const Value &vLength);

    String stringSprintf(VM *, const Value &thisObj, size_t argCount, Value *args);

    // TODO: need impl RegExp class
    // String stringReplace(VM *, const Value &thisObj, size_t argCount, const Value &p1);

    String stringEscape(VM *, const Value &thisObj, size_t argCount);

    // TODO: need impl Array class
    // String Array stringSplit(VM *, const Value &thisObj, size_t, const String *pattern, const Value &vReserved, const
    // Value &vPurgeEmpty);

    String stringTrim(VM *, const Value &thisObj, size_t);

    void stringReverse(VM *, const Value &thisObj, size_t);

    String stringRepeat(VM *, const Value &thisObj, size_t, Integer count);

} // namespace cial::stdlib
