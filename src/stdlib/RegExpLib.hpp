// Copyright (c) 2026/10/24
//
// RegExp 内建方法（TJS2 tjsRegExp.cpp 语义）：test/match/exec/compile/replace/split；
// 以及 String.split / Array.split 的分派实现。位置均为字节偏移（UTF-8 码点语义记 TODO）。
// replace 的函数回调（TJS2 支持以函数为替换串）待脚本回调调用机制，暂仅字符串替换。
#pragma once

#include "types/RegExp.hpp"
#include "types/String.hpp"
#include "types/Value.hpp"

namespace cial {
    class Context;
    class VM;
} // namespace cial

namespace cial::stdlib {

    void registerRegExpLib(Context &context);

    // /re/.test(target)：搜索并更新匹配状态（g 时推进 start），返回是否命中
    Value regexpTest(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // /re/.match(target)：同 test 但不改任何状态；返回 [整体, 组1, ...]，未命中为空数组
    Value regexpMatch(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // /re/.exec(target)：搜索 + 更新状态（index/lastIndex/lastMatch/... 与 matches）
    Value regexpExec(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // /re/.compile(pattern, flags?)
    void regexpCompile(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // /re/.replace(target, replacement)：g 全局替换；忽略 start、不改状态
    Value regexpReplace(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // /re/.split(target, reserved, purgeempty)
    Value regexpSplit(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // "str".split(pattern, reserved, purgeempty)：pattern 为 RegExp 走正则切分，
    // 否则按分隔字符集切分（TJS2 Array.split 字符串模式语义）
    Value stringSplitWith(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // [].split(pattern, string, reserved, purgeempty)：清空并填充 this 数组，返回 this
    Value arraySplitWith(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // String.replace / RegExp.replace 共用替换核心（TJS2 replace_regex；忽略 start）。
    // replacement 为字符串或函数（函数每次匹配收到 [整体,组...] 数组，返回替换串）
    String regexpReplaceIn(VM *vm, const RegExp *re, const String &target, const Value &replacement);

} // namespace cial::stdlib
