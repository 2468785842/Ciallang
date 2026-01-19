// ReSharper disable CppDFAConstantFunctionResult
// ReSharper disable CppDFAUnreachableCode
#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"
#include "runtime/Context.hpp"
#include "runtime/Runtime.hpp"

using namespace cial;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if(size == 0)
        return 0;
    std::string input{ reinterpret_cast<const char *>(data), size };

    Runtime rt{};
    Context context{ rt };
    Common::SourceFile sourceFile;
    Common::Result r{};
    sourceFile.load(r, input);

    Syntax::Parser parser{ sourceFile, context.pp() };
    try {
        parser.parse(r);
    } catch(...) {
        std::abort();
    }
    return 0;
}