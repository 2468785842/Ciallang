/*
 * Copyright (c) 2024/5/6 下午8:16
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */

#include "common/SourceFile.hpp"
#include "gen/IRGenerator.hpp"
#include "parser/Parser.hpp"
#include "standard/Print.hpp"
#include "vm/VMState.hpp"

int main(int argc, char **argv) {
    if(argc < 2) {
        fmt::println("Usage: cll <script-file>");
        return 0;
    }

    Ciallang::Common::Result r{};

    Ciallang::Common::SourceFile sourceFile{ argv[1] };
    sourceFile.load(r);

    Ciallang::Syntax::AstBuilder astBuilder{};
    Ciallang::Syntax::Parser parser{ sourceFile, astBuilder };
    auto *globalNode = parser.parse(r);

    Ciallang::Inter::SymbolTable globalTable;
    Ciallang::Inter::IRGenerator codeGen{ sourceFile, globalTable };

    auto chunk = codeGen.parseAst(r, globalNode);

    Ciallang::Bytecode::VMState interpreter{ globalTable };
    // interpreter.global(&Ciallang::Standard::S_PrintFunction);
    interpreter.global("println", Ciallang::Standard::S_PrintlnFunction);

    fmt::println("{}", interpreter.dumpInstruction(*chunk));

    auto start = std::chrono::high_resolution_clock::now();
    interpreter.allocCallFrame(chunk.get());
    interpreter.run();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::nanoseconds>(end - start);

    fmt::println("{}", interpreter.dumpRegisters());

    fmt::println("Time taken by function: {}ns", duration.count());

    return 0;
}
