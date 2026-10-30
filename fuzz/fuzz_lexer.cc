#include <cstdlib>
#include <string>
#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "parser/Lexer.hpp"
#include "parser/PreProcessor.hpp"

using namespace cial;

extern "C" int LLVMFuzzerTestOneInput(const u8 *data, size_t size) {
    std::string input{ reinterpret_cast<const char *>(data), size };
    PreProcessor preProcessor;
    Common::SourceFile sourceFile;
    Common::Result result;
    sourceFile.load(result, input);
    if(result.isFailed())
        return 0;

    syntax::Lexer lexer{ sourceFile, preProcessor };
    try {
        while(lexer.hasNext()) {
            syntax::Token *token{};
            lexer.next(token);
            syntax::Token consumed;
            lexer.takeOverToken(consumed);
        }
    } catch(...) {
        std::abort();
    }
    return 0;
}
