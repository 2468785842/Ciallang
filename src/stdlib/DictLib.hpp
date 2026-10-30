// Copyright (c) 2026/10/24
//
// Dictionary 内建方法（经 NativeRegister TypeId::Dictionary 注册）。
#pragma once

#include "types/Value.hpp"

namespace cial {
    class Context;
    class VM;
}

namespace cial::stdlib {

    void registerDictLib(Context &context);

    // assign(src)：把 src 字典内容浅拷贝到 this（替换现有成员）
    void dictAssign(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // clear()：清空全部成员
    void dictClear(VM *vm, const Value &thisObj, size_t argCount);

    // erase(key)：删除键，返回是否删除
    Value dictErase(VM *vm, const Value &thisObj, size_t argCount, Value *args);

} // namespace cial::stdlib
