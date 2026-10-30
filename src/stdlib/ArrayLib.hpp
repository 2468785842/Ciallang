// Copyright (c) 2026/10/24
//
// Array 内建方法（经 NativeRegister TypeId::Array 注册）。
// 变参方法用 Value* 形态（argCount 运行期已知）；thisObj 为 Array 对象。
#pragma once

#include "types/Value.hpp"

namespace cial {
    class Context;
    class VM;
}

namespace cial::stdlib {

    void registerArrayLib(Context &context);

    // push(...values)：尾部追加，返回新 count
    Value arrayPush(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // pop()：移除并返回末元素（空数组返回 void）
    Value arrayPop(VM *vm, const Value &thisObj, size_t argCount);

    // unshift(...values)：头部插入，返回新 count
    Value arrayUnshift(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // shift()：移除并返回首元素（空数组返回 void）
    Value arrayShift(VM *vm, const Value &thisObj, size_t argCount);

    // insert(index, ...values)：指定位置插入
    Value arrayInsert(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // remove(index)：移除指定位置元素
    Value arrayRemove(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // erase()：清空全部元素
    void arrayErase(VM *vm, const Value &thisObj, size_t argCount);

    // reverse()：原地反转
    void arrayReverse(VM *vm, const Value &thisObj, size_t argCount);

    // join(delim = ",")：字符串连接
    Value arrayJoin(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    // sort(order|comparator = '+', stable = false)：原地排序，无返回值（TJS2 语义）
    Value arraySort(VM *vm, const Value &thisObj, size_t argCount, Value *args);

} // namespace cial::stdlib
