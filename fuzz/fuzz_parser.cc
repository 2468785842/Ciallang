#include <cstdlib>
#include <string>
#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "parser/Parser.hpp"
#include "parser/PreProcessor.hpp"

using namespace cial;

extern "C" int LLVMFuzzerTestOneInput(const u8 *data, size_t size) {
    std::string input{ reinterpret_cast<const char *>(data), size };

    PreProcessor preProcessor;
    Common::SourceFile sourceFile;
    Common::Result r{};
    sourceFile.load(r, input);
    if(r.isFailed())
        return 0;

    syntax::Parser parser{ sourceFile, preProcessor };
    try {
        parser.parse(r);
    } catch(...) {
        std::abort();
    }
    return 0;
}
