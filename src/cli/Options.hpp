#pragma once

#include <string>

#include "vm/IntegerJit.hpp"

namespace cial::cli {
    enum class Mode { Script, Bytecode, CheckJson, DumpCfg, DumpSsa, Repl };

    struct Options {
        Mode mode{ Mode::Script };
        std::string input;
        std::string traceOutput;
        // `-` 输入时诊断使用的逻辑名（LSP 等宿主保留原文档身份，不暴露临时路径）
        std::string sourceName;
        vm::JitMode jit{ vm::JitMode::Off };
        int jsonSchema{ 2 };
        bool tailRecursion{};
        bool timing{};
        bool dumpTac{};
        bool noColor{};
    };

    Options parseOptions(int argc, char **argv);
} // namespace cial::cli
