/*
 * Copyright (c) 2024/5/30 上午9:14
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
#include <gen/CodeGen.hpp>
#include <gtest/gtest.h>

#include <vm/VMChunk.hpp>
#include <vm/Instruction.hpp>

#include <init/GlogInit.hpp>
#include <parser/Parser.hpp>

#include "common/SourceFile.hpp"

using namespace Ciallang::VM;

int main(int argc, char** argv) {
    // Initialize Google's logging library.
    Ciallang::Init::InitializeGlog(argv);
    // Initialize GoogleTest.
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

TEST(Interpreter, Execute) {
    Ciallang::Common::Result r{};

    Ciallang::Common::SourceFile sourceFile{ R"(.\startup.tjs)" };
    sourceFile.load(r);
    CHECK(!r.isFailed());

    Ciallang::Syntax::AstBuilder astBuilder{};
    Ciallang::Syntax::Parser parser{ sourceFile, astBuilder };
    auto* globalNode = parser.parse(r);

    LOG(WARNING) << "Parser";
    if(!globalNode) {
        for(const auto& message : r.messages())
            LOG(ERROR) << message.message();
        CHECK(false);
    }

    VMChunk vmChunk{ sourceFile.path() };
    Ciallang::Inter::CodeGen codeGen{ sourceFile, &vmChunk };
    codeGen.loadAst(r, globalNode);

    LOG(WARNING) << "CodeGen";
    if(r.isFailed()) {
        for(const auto& message : r.messages())
            LOG(ERROR) << message.message();
        CHECK(false);
    }

    Interpreter interpreter{ sourceFile, &vmChunk };
    interpreter.run(r);

    LOG(WARNING) << "Interpreter";
    if(r.isFailed()) {
        for(const auto& message : r.messages())
            LOG(ERROR) << message.message();
        CHECK(false);
    }
}
