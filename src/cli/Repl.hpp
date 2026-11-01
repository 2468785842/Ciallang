#pragma once

#include <iosfwd>
#include <string>

#include "vm/VMState.hpp"

namespace cial::cli {
    int runRepl(vm::VMState &state, std::istream &input, bool interactive);
    bool isInteractiveInput();
} // namespace cial::cli
