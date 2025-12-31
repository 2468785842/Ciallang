// Copyright (c) 2025/9/29 13:01
//
// /\  _` \   __          /\_ \  /\_ \
// \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
//  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
//   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
//    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
//     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
//                                                            /\____
//                                                            \_/__/
//

//
// Created by LiDon on 2025/9/29.
//

#include <catch.hpp>
#include <iostream>

#include "test_config.h"

#include "common/SourceFile.hpp"
#include "src/parser/Lexer.hpp"

TEST_CASE("词法分析 - 基本功能") {
    Cial::Common::SourceFile source_file{ TEST_FILES_PATH "/startup.tjs" };
    Cial::Common::Result r{};
    source_file.load(r);

    // 如果文件不存在，跳过测试
    if(r.isFailed()) {
        WARN("Test file not found, skipping lexer test");
        return;
    }

    Cial::Syntax::Lexer lexer{ source_file };
    int tokenCount = 0;

    while(lexer.hasNext()) {
        Cial::Syntax::Token *token = nullptr;
        lexer.next(token);
        REQUIRE(token != nullptr);

        std::cout << token->name() << "\t";
        if(token->value().type() != Cial::ValueType::Octet) {
            if(token->value().type() == Cial::ValueType::Integer) {
                std::cout << std::dec << token->value().toInteger();
            } else if(token->value().type() == Cial::ValueType::Real) {
                std::cout << std::dec << token->value().toReal();
            } else if(token->value().type() == Cial::ValueType::String)
                std::cout << "\"" << token->value().toString() << "\"";
        }
        if(token->type() == Cial::Syntax::TokenType::Invalid) {
            std::cout << token->location.start().line << "," << token->location.start().column;
        }
        std::cout << std::endl;
        tokenCount++;
    }

    // 验证至少读取了一些token
    REQUIRE(tokenCount > 0);

    if(r.isFailed()) {
        for(const auto &item : r.messages()) {
            std::cerr << item.details() << std::endl;
            std::cerr << item.message() << std::endl;
        }
    }
}