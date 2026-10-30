#pragma once

#include "common/Result.hpp"

namespace cial::cli {
    void printDiagnostics(const Common::Result &diagnostics);
    void printJsonDiagnostics(const Common::Result &diagnostics);
} // namespace cial::cli
