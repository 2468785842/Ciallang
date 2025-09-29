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

#include "pch.h"

#include "common/SourceFile.hpp"
#include "gen/BytecodeGen.hpp"
#include "parser/Parser.hpp"
#include "standard/print.hpp"
#include "vm/Interpreter.hpp"

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

    Ciallang::Inter::BytecodeGen codeGen{ sourceFile };

    auto chunk = codeGen.parseAst(r, globalNode);

    Ciallang::Bytecode::Interpreter interpreter{};
    interpreter.global(&Ciallang::Standard::S_PrintFunction);
    interpreter.global(&Ciallang::Standard::S_PrintlnFunction);

    fmt::println("{}", interpreter.dumpInstruction(*chunk));
    auto start = std::chrono::high_resolution_clock::now();
    interpreter.run(chunk.get());
    fmt::println("{}", interpreter.dumpRegisters());
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::microseconds>(end - start);

    fmt::println("Time taken by function: {}ms", duration.count());

    return 0;
}
