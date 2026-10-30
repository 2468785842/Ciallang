#pragma once

#include <string>

#include "vm/IntegerJit.hpp"

namespace cial::cli {
    enum class Mode { Script, Bytecode, CheckJson, DumpCfg, DumpSsa, Repl };

    struct Options {
        Mode mode{ Mode::Script };
        std::string input;
        std::string traceOutput;
        vm::JitMode jit{ vm::JitMode::Off };
        bool tailRecursion{};
        bool timing{};
        bool dumpTac{};
    };

    Options parseOptions(int argc, char **argv);
} // namespace cial::cli
