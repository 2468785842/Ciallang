/*
 * Copyright (c) 2026/10/26
 *
 * TJS2 字节码文件读取（阶段 1：纯格式解析）单元测试。
 * 语料：tests/test_files/tjsbc/*.tjsbc 由参考实现
 * `TJSCompiler --dump script.tjs out.tjsbc` 生成（生成脚本见批次提交说明）。
 */

#include <catch.hpp>

#include <cstring>

#include "test_config.h"

#include "tjsbc/TjsBytecodeReader.hpp"

using namespace cial;
using namespace cial::tjsbc;

TEST_CASE("TJS2 字节码读取——参考实现导出语料") {
    SECTION("s1：整数/函数 context") {
        const auto file = readTjsBytecodeFile(TEST_FILES_PATH R"(/tjsbc/s1.tjsbc)");
        REQUIRE(file.fileSize == 484);
        // 字符串段含驻留的 global / 成员名 / 字符串字面量
        bool hasGlobal = false, hasF = false, hasB = false;
        for(const auto &str : file.pool.strings) {
            if(str == "global"_str)
                hasGlobal = true;
            if(str == "f"_str)
                hasF = true;
            if(str == "b"_str)
                hasB = true;
        }
        REQUIRE(hasGlobal);
        REQUIRE(hasF);
        REQUIRE(hasB);
        // 整数段含字面量 2（x * 2）
        // 值域分型：小整数进 BYTE 段（|v|<=127），2 ∈ BYTE
        bool hasTwo = false;
        for(const auto v : file.pool.bytes)
            if(v == 2)
                hasTwo = true;
        REQUIRE(hasTwo);
        // 至少 2 个 context（顶层 + 函数 f）；topLevel 指向有效索引
        REQUIRE(file.contexts.size() >= 2);
        REQUIRE(file.topLevel >= 0);
        REQUIRE(file.topLevel < static_cast<i32>(file.contexts.size()));
        // 顶层 context 类型
        REQUIRE(file.contexts[static_cast<size_t>(file.topLevel)].type == TjsContextType::TopLevel);
        // 函数 context 存在且签名正确（1 个声明参数）
        bool hasFunc = false;
        for(const auto &ctx : file.contexts) {
            if(ctx.type == TjsContextType::Function) {
                hasFunc = true;
                REQUIRE(ctx.funcDeclArgCount == 1);
                REQUIRE(ctx.maxVariableCount > 0);
            }
        }
        REQUIRE(hasFunc);
    }

    SECTION("s2：实数/长整数/中文字符串/octet") {
        const auto file = readTjsBytecodeFile(TEST_FILES_PATH R"(/tjsbc/s2.tjsbc)");
        bool hasPi = false, hasBig = false, hasCjk = false, hasDead = false;
        for(const auto v : file.pool.reals)
            if(v == 3.14)
                hasPi = true;
        for(const auto v : file.pool.longs)
            if(v == 5000000000LL)
                hasBig = true; // 超 int32 → LONG 段
        for(const auto &str : file.pool.strings)
            if(str == "中文"_str)
                hasCjk = true; // UTF-16LE → UTF-8 转换
        for(const auto &o : file.pool.octets)
            if(o.size() == 2 && o[0] == 0xDE && o[1] == 0xAD)
                hasDead = true;
        REQUIRE(hasPi);
        REQUIRE(hasBig);
        REQUIRE(hasCjk);
        REQUIRE(hasDead);
    }

    SECTION("s3：class / property context 结构") {
        const auto file = readTjsBytecodeFile(TEST_FILES_PATH R"(/tjsbc/s3.tjsbc)");
        bool hasClass = false, hasGetter = false;
        for(const auto &ctx : file.contexts) {
            if(ctx.type == TjsContextType::Class)
                hasClass = true;
            if(ctx.type == TjsContextType::PropertyGetter)
                hasGetter = true;
        }
        REQUIRE(hasClass);
        REQUIRE(hasGetter);
        // 成员函数表挂在其所属 context 上（此语料：Function `get` 携带 1 条）
        bool memberOnFunc = false;
        for(const auto &ctx : file.contexts) {
            if(ctx.type == TjsContextType::Function && ctx.propMembers.size() == 1)
                memberOnFunc = true;
        }
        REQUIRE(memberOnFunc);
        // data 槽的 INTER_OBJECT 指向有效 context 索引
        for(const auto &ctx : file.contexts) {
            for(const auto &slot : ctx.data) {
                if(slot.type == TJS_TYPE_INTER_OBJECT)
                    REQUIRE(slot.index >= 0);
            }
        }
    }

    SECTION("码字保持文件态（i16、未换算）") {
        const auto file = readTjsBytecodeFile(TEST_FILES_PATH R"(/tjsbc/s1.tjsbc)");
        for(const auto &ctx : file.contexts)
            REQUIRE_FALSE(ctx.code.empty()); // 顶层与函数均有码字
    }
}

TEST_CASE("TJS2 字节码读取——畸形输入拒绝") {
    SECTION("魔数不符") {
        Vec<u8> bytes{'X', 'Y', 'Z', 'Z', '1', '0', '0', 0};
        REQUIRE_THROWS(readTjsBytecodeBytes(bytes));
    }

    SECTION("filesize 不一致") {
        Vec<u8> bytes{'T', 'J', 'S', '2', '1', '0', '0', 0, 99, 0, 0, 0};
        REQUIRE_THROWS(readTjsBytecodeBytes(bytes));
    }

    SECTION("截断（头后即止）") {
        Vec<u8> bytes{'T', 'J', 'S', '2', '1', '0', '0', 0};
        REQUIRE_THROWS(readTjsBytecodeBytes(bytes));
    }

    SECTION("手工构造最小合法文件可解析") {
        // 头(12) + DATA(tag4+size4+count0×7段) + OBJS(tag4+size4+toplevel4+count4)
        Vec<u8> bytes{};
        auto put32 = [&bytes](const i32 v) {
            for(int k = 0; k < 4; ++k)
                bytes.push_back(static_cast<u8>(v >> (k * 8) & 0xFF));
        };
        auto putTag = [&bytes](const char *t) {
            bytes.insert(bytes.end(), t, t + 4);
        };
        const u8 header[8] = {'T', 'J', 'S', '2', '1', '0', '0', 0};
        bytes.insert(bytes.end(), header, header + 8);
        put32(0); // filesize 占位，末尾回填
        putTag("DATA");
        const i32 dataArea = 7 * 4; // 7 段 × (count=0 的 i32)
        put32(8 + dataArea); // datasize 含 DATA tag(4) + size 字段(4)
        for(int k = 0; k < 7; ++k)
            put32(0); // 每段 count=0
        putTag("OBJS");
        put32(12); // objsize = 8 + 4 + 4（toplevel + count）
        put32(-1); // toplevel：无
        put32(0);  // context 数 0
        // 回填 filesize（覆盖偏移 8 的占位字段）
        const i32 fileSize = static_cast<i32>(bytes.size());
        for(int k = 0; k < 4; ++k)
            bytes[8 + k] = static_cast<u8>(fileSize >> (k * 8) & 0xFF);
        const auto file = readTjsBytecodeBytes(bytes);
        REQUIRE(file.topLevel == -1);
        REQUIRE(file.contexts.empty());
        REQUIRE(file.pool.ints.empty());
    }
}
