#pragma once

#include "common/SourceFile.hpp"
#include "runtime/Context.hpp"
#include "vm/Chunk.hpp"
#include "vm/VMState.hpp"

namespace cial::cli {
    struct Compilation {
        Opt<vm::Chunk> chunk;
        OptReg result;
    };

    Compilation compile(Common::Result &diagnostics, Context &context, Common::SourceFile &source,
                        bool expression = false);
    Value execute(vm::VMState &state, vm::Chunk chunk, OptReg result = {});
    void dumpGraphs(Runtime &runtime, vm::Chunk &chunk, bool ssa);
} // namespace cial::cli
