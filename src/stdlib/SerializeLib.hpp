// Copyright (c) 2026/10/26
//
// Array/Dictionary 结构化存取与 Octet pack 系（TJS2 tjsBinarySerializer +
// tjsArray.cpp/tjsDictionary.cpp 的 save/load 系 + tjsOctPack 对齐）。
#pragma once

#include "types/Array.hpp"
#include "types/Dictionary.hpp"
#include "types/Value.hpp"

namespace cial {
    class Context;

    class VM;

    namespace stdlib {

        void registerSerializeLib(Context &context);

        // ---- Array（tjsArray.cpp）----
        // load：文本逐行读入（行分隔 \r\n|\r|\n，元素一律字符串）；返回 objthis
        Value arrayLoad(VM *vm, const Value &thisObj, size_t argCount, Value *args);
        // save：文本逐行写出（仅 string/integer/real 写值，void/对象只写换行）；返回 objthis
        Value arraySave(VM *vm, const Value &thisObj, size_t argCount, Value *args);
        // loadStruct：仅二进制（"KBAD100\0" 头）读入自身；非二进制报错；返回 objthis
        Value arrayLoadStruct(VM *vm, const Value &thisObj, size_t argCount, Value *args);
        // saveStruct：mode 含 'b' 走二进制（头 + 结构化），否则文本结构化；返回 objthis
        Value arraySaveStruct(VM *vm, const Value &thisObj, size_t argCount, Value *args);
        // pack：模板 + 数组元素 → Octet（tjsOctPack）
        Value arrayPack(VM *vm, const Value &thisObj, size_t argCount, Value *args);
        // assignStruct：结构化深拷贝（嵌套 Array/Dictionary 递归新建，环 → null）
        Value arrayAssignStruct(VM *vm, const Value &thisObj, size_t argCount, Value *args);

        // ---- Dictionary（tjsDictionary.cpp）----
        // load/save：原版即 TODO 空实现（no-op），按特性同步
        void dictLoad(VM *vm, const Value &thisObj, size_t argCount, Value *args);
        void dictSave(VM *vm, const Value &thisObj, size_t argCount, Value *args);
        // loadStruct：二进制读入自身；非二进制回落文本结构化解析（解析值为返回值，
        // 不写入自身——原版同款怪癖）
        Value dictLoadStruct(VM *vm, const Value &thisObj, size_t argCount, Value *args);
        // saveStruct：mode 含 'b' 走二进制，否则文本结构化；返回 objthis
        Value dictSaveStruct(VM *vm, const Value &thisObj, size_t argCount, Value *args);
        Value dictAssignStruct(VM *vm, const Value &thisObj, size_t argCount, Value *args);

        // ---- Octet ----
        // unpack：模板 → 数组（TJS2 Octet 仅有的方法）
        Value octetUnpack(VM *vm, const Value &thisObj, size_t argCount, Value *args);

    } // namespace cial::stdlib

} // namespace cial
