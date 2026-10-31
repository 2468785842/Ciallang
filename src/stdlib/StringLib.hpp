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

#include "types/RegExp.hpp"
#include "types/String.hpp"
#include "types/Value.hpp"

namespace cial {
    class Context;
    class VM;
} // namespace cial

namespace cial::stdlib {

    void registerStringLib(Context &context);

    String stringCharAt(VM *, const Value &thisObj, size_t argCount, Integer index);

    Integer stringIndexOf(VM *, const Value &thisObj, size_t argCount, const String *subStr, const Value &vStart);

    // TJS2 语义：返回新串，不改原串（原实现原地修改与语料 test_string 冲突）
    String stringToUpperCase(VM *, const Value &thisObj, size_t argCount);

    String stringToLowerCase(VM *, const Value &thisObj, size_t argCount);

    String stringSubstring(VM *, const Value &thisObj, size_t argCount, Integer start, const Value &vLength);

    String stringSprintf(VM *, const Value &thisObj, size_t argCount, Value *args);

    // replace(regexp, replacement)：g 标志全局替换、否则仅第一处（TJS2 语义；
    // 实现委托 RegExpLib::regexpReplaceIn）；split 见 RegExpLib::stringSplitWith
    String stringReplace(VM *, const Value &thisObj, size_t argCount, const Value &re, const Value &replacement);

    String stringEscape(VM *, const Value &thisObj, size_t argCount);

    String stringTrim(VM *, const Value &thisObj, size_t);

    String stringReverse(VM *, const Value &thisObj, size_t);

    String stringRepeat(VM *, const Value &thisObj, size_t, Integer count);

} // namespace cial::stdlib
