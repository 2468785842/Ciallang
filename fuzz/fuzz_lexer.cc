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
    {
        std::string input{ reinterpret_cast<const char *>(data), size };
        Runtime rt{ 0 };
        Context context{ rt };
        Common::SourceFile sourceFile;
        Common::Result r{};
        sourceFile.load(r, input);

        if(r.isFailed())
            return 0;
        Syntax::Lexer lexer{ sourceFile, context.pp() };
        try {
            while(lexer.hasNext()) {
                Syntax::Token *t;
                lexer.next(t);
                Syntax::Token take;
                lexer.takeOverToken(take);
            }
        } catch(...) {
            std::abort();
        }
    }
    return 0;
}