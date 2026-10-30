#include "stdlib/StandardLibrary.hpp"
#include "vm/VM.hpp"

int main() {
    using namespace cial;
    Runtime runtime;
    Context context{ runtime };
    stdlib::registerStandardLibrary(context);
    vm::VMState state{ context };
    VM vm{ &state };
    return *vm.evalExpr<Integer>("40 + 2"_str) == 42 ? 0 : 1;
}
