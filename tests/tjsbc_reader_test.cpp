/*
 * Copyright (c) 2026/10/26
 *
 * TJS2 字节码文件读取（阶段 1：纯格式解析）单元测试。
 * 参考实现导出语料（s1-s5）无源码归档，已按"无源码 fixture 清理"约定移除；
 * 保留畸形输入拒绝与手工构造用例，语料级覆盖见 tjsbc_translator_test.cpp
 * （s6_class/s7_ext/test_* 均有源码，可用 `TJSCompiler --dump` 再生成）。
 */

#include <catch2/catch_all.hpp>

#include <cstring>

#include "test_config.h"

#include "tjsbc/TjsBytecodeReader.hpp"

using namespace cial;
using namespace cial::tjsbc;

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
