// Copyright (c) 2026/10/26
//
// TJS2 字节码文件（"TJS2" "100\0"，tjs2-bytecode.md §5）的纯格式解析。
// 转译路线（doc/bytecode/mapping.md）阶段 1：只按格式读出内存模型，
// 不做任何语义（寄存器/码字保持文件态 i16 索引形态，地址换算归阶段 2 转译器）。
// 读取行为对齐参考实现 tjsByteCodeLoader.cpp（ReadDataArea/ReadObjects）。
#pragma once

#include <string>
#include <vector>

#include "types/String.hpp"
#include "types/Types.hpp"

namespace cial::tjsbc {

    // 常量槽类型 tag（tjsConstArrayData.h）
    enum TjsConstSlotType : i16 {
        TJS_TYPE_VOID = 0,
        TJS_TYPE_OBJECT = 1, // 仅允许 null 闭包
        TJS_TYPE_INTER_OBJECT = 2, // idx → context 索引
        TJS_TYPE_STRING = 3,
        TJS_TYPE_OCTET = 4,
        TJS_TYPE_REAL = 5,
        TJS_TYPE_BYTE = 6,
        TJS_TYPE_SHORT = 7,
        TJS_TYPE_INTEGER = 8,
        TJS_TYPE_LONG = 9,
    };

    // context 类型（tjsInterCodeGen.h tTJSContextType）
    enum class TjsContextType : i32 {
        TopLevel = 0,
        Function = 1,
        ExprFunction = 2,
        Property = 3,
        PropertySetter = 4,
        PropertyGetter = 5,
        Class = 6,
        SuperClassGetter = 7,
    };

    // 常量池 7 段（tjsConstArrayData::ExportBuffer 固定顺序；段内已去重）
    struct TjsConstPool {
        Vec<i8> bytes{};
        Vec<i16> shorts{};
        Vec<i32> ints{};
        Vec<i64> longs{};
        Vec<double> reals{};
        Vec<String> strings{}; // UTF-8（由 UTF-16LE 转换）
        Vec<Vec<u8>> octets{};
    };

    // 常量槽（context 的 DataArea 条目：type + 段内下标）
    struct TjsDataSlot {
        i16 type;
        i16 index;
    };

    // 单个 context（tjsInterCodeContext::ExportByteCode 的记录）
    struct TjsContext {
        i32 parent = -1; // 父 context 索引（顶层 -1）
        i32 name = -1; // 常量池 STRING 索引（-1 无名）
        TjsContextType type = TjsContextType::TopLevel;
        i32 maxVariableCount = 0; // ┐
        i32 variableReserveCount = 0; // │帧尺寸（ra 基偏移 K = VC+RC）
        i32 maxFrameCount = 0; // ┘
        i32 funcDeclArgCount = 0; // ┐
        i32 funcDeclUnnamedArgArrayBase = 0; // │函数签名
        i32 funcDeclCollapseBase = -1; // -1 = 无 collapse             // ┘
        i32 propSetter = -1; // context 索引（property setter）
        i32 propGetter = -1;
        i32 superClassGetter = -1;
        Vec<i32> codePos{}; // 仅 outputdebug 产物非空
        Vec<i32> sourcePos{};
        Vec<i16> code{}; // 文件态 i16 码字（未换算）
        Vec<TjsDataSlot> data{}; // 常量槽
        Vec<i32> superClassGetterPointers{}; // scgetter 代码地址（已索引化）
        Vec<std::pair<i32, i32>> propMembers{}; // 成员函数表 (name STRING idx, context idx)
    };

    // 整个字节码文件
    struct TjsBytecodeFile {
        i32 fileSize = 0;
        i32 dataSize = 0;
        i32 objsSize = 0;
        i32 topLevel = -1; // 顶层 context 索引（无则 -1）
        TjsConstPool pool{};
        Vec<TjsContext> contexts{};
    };

    // 解析失败抛 std::runtime_error（格式/越界/魔数不符）
    TjsBytecodeFile readTjsBytecodeFile(const std::string &path);
    TjsBytecodeFile readTjsBytecodeBytes(const Vec<u8> &bytes);

} // namespace cial::tjsbc
