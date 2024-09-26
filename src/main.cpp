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

#include "lexer/Lexer.hpp"
#include "common/SourceFile.hpp"
#include "gen/BytecodeGen.hpp"
#include "init/GlogInit.hpp"
#include "parser/Parser.hpp"
#include "parser/Parser.hpp"
#include "vm/Interpreter.hpp"
#include "core/print.hpp"

void testLexer() {
    Ciallang::Common::SourceFile source_file{ R"(.\startup.tjs)" };
    Ciallang::Common::Result r{};
    source_file.load(r);
    Ciallang::Syntax::Lexer lexer{ source_file };
    while(lexer.hasNext()) {
        Ciallang::Syntax::Token* token = nullptr;
        lexer.next(token);

        std::cout << token->name() << "\t";
        if(token->value() != nullptr) {
            if(token->value()->type() != Ciallang::TjsValueType::Octet) {
                if(token->value()->type() == Ciallang::TjsValueType::Integer) {
                    std::cout << std::dec << token->value()->asInteger();
                } else if(token->value()->type() == Ciallang::TjsValueType::Real) {
                    std::cout << std::dec << token->value()->asReal();
                } else if(token->value()->type() == Ciallang::TjsValueType::String)
                    std::cout << "\"" << token->value()->asString() << "\"";
            }
        }
        if(token->type() == Ciallang::Syntax::TokenType::Invalid) {
            std::cout << token->location.start().line << "," << token->location.start().column;
        }
        std::cout << std::endl;
    }
    if(r.isFailed()) {
        for(const auto& item : r.messages()) {
            std::cerr << item.details() << std::endl;
            std::cerr << item.message() << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    Ciallang::Init::InitializeGlog(argc, argv);

    LOG_IF(FATAL, argc > 2) << "Usage: Ciallang <file-name>";
    Ciallang::Common::Result r{};

    Ciallang::Common::SourceFile sourceFile{ argv[1] };
    sourceFile.load(r);

    Ciallang::Syntax::AstBuilder astBuilder{};
    Ciallang::Syntax::Parser parser{ sourceFile, astBuilder };
    auto* globalNode = parser.parse(r);

    Ciallang::Inter::BytecodeGen codeGen{ sourceFile };
    auto chunk = codeGen.parseAst(r, globalNode);

    Ciallang::Bytecode::Interpreter interpreter{};
    interpreter.global("println", Ciallang::TjsValue{ Ciallang::Core::S_PrintlnFunction});

    fmt::println("{}", interpreter.dumpInstruction(*chunk));
    auto start = std::chrono::high_resolution_clock::now();
    interpreter.run(chunk.get());
    fmt::println("{}", interpreter.dumpRegisters());
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(end - start);

    fmt::println("Time taken by function: {}s", duration.count());

    // testLexer();
    /*Ciallang::Inter::Compiler compiler{
            Ciallang::Inter::CompilerOptions{
            },
            R"(.\startup.tjs)"
    };
    compiler.initializeCoreTypes();
    compiler.compile();
    // compiler.elements();
    // compiler.writeCodeDomGraph(R"(.\startup.dom.dot)");
    for(const auto& message : compiler.result().messages()) {
        if(message.isError()) {
            fmt::println("{}", message.message());
            if(!message.details().empty())
                fmt::println("{}", message.details());
            fmt::println("");
        }
    }*/
    return 0;
}
