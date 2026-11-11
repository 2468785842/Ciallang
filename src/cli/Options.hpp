#pragma once

#include <string>

#include "vm/IntegerJit.hpp"

namespace cial::cli {
    enum class Mode {
        Script,
        Bytecode,
        CheckJson,
        DeclJson,
        DumpCfg,
        DumpSsa,
        Decompile,
        Repl,
        Capabilities,
        ApiJson,
        ApiDecl
    };

    struct Options {
        Mode mode{ Mode::Script };
        std::string input;
        std::string traceOutput;
        // --api-decl-dir（与 --api-decl 同用）：按顶层符号拆分写出到该目录，
        // 供构建期声明分发（doc/declarations.md §3）；空 = 单文件走 stdout。
        std::string apiDeclDir;
        // `-` 输入时诊断使用的逻辑名（LSP 等宿主保留原文档身份，不暴露临时路径）
        std::string sourceName;
        vm::JitMode jit{ vm::JitMode::Off };
        int jsonSchema{ 2 };
        bool tailRecursion{};
        bool timing{};
        bool dumpTac{};
        bool noColor{};
        // --decompile（doc/bytecode/decompiler-plan.md §8）：严格模式默认；--view 输出
        // 反汇编级诊断视图；--map 写行表 JSON；--source 提供原源码做偏移→行号换算
        bool decompileView{};
        std::string decompileMap;
        std::string decompileSource;
    };

    Options parseOptions(int argc, char **argv);
} // namespace cial::cli
