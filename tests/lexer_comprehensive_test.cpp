/*
 * Copyright (c) 2024/12/18
 *
 * 词法分析器全面单元测试
 */

#include <catch.hpp>
#include <string>
#include <vector>

#include "types/TjsString.hpp"

#include "common/SourceFile.hpp"
#include "lexer/Lexer.hpp"

using namespace Ciallang;

TEST_CASE("词法分析器 - 关键字识别") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("基本关键字") {
        sourceFile.load(r, "function if else while return break continue");
        Syntax::Lexer lexer{ sourceFile };

        std::vector<std::string> expectedTokens = { "function", "if",    "else",     "while",
                                                    "return",   "break", "continue", "end_of_file" };

        for(const auto &expected : expectedTokens) {
            Syntax::Token *token = nullptr;
            REQUIRE(lexer.hasNext());
            lexer.next(token);
            REQUIRE(token != nullptr);
            REQUIRE(token->name() == expected);
        }
        REQUIRE_FALSE(lexer.hasNext());
    }
}

TEST_CASE("词法分析器 - 标识符识别") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("有效标识符") {
        sourceFile.load(r, "variable_name _underscore camelCase123");
        Syntax::Lexer lexer{ sourceFile };

        std::vector<std::string> expectedIdentifiers = { "variable_name", "_underscore", "camelCase123" };

        for(const auto &expected : expectedIdentifiers) {
            Syntax::Token *token = nullptr;
            REQUIRE(lexer.hasNext());
            lexer.next(token);
            REQUIRE(token != nullptr);
            REQUIRE(*token->value()->toString() == expected);
        }
    }
}

TEST_CASE("词法分析器 - 字面量识别") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("整数字面量") {
        sourceFile.load(r, "123 456 0 999");
        Syntax::Lexer lexer{ sourceFile };

        std::vector<TjsInteger> expectedValues = { 123, 456, 0, 999 };

        for(const auto &expected : expectedValues) {
            Syntax::Token *token = nullptr;
            REQUIRE(lexer.hasNext());
            lexer.next(token);
            REQUIRE(token != nullptr);
            REQUIRE(token->value() != nullptr);
            REQUIRE(token->value()->type() == TjsValueType::Integer);
            REQUIRE(token->value()->toInteger() == expected);
        }
    }

    SECTION("浮点数字面量") {
        sourceFile.load(r, "3.14 2.718 0.5");
        Syntax::Lexer lexer{ sourceFile };

        std::vector<TjsReal> expectedValues = { 3.14, 2.718, 0.5 };

        for(const auto &expected : expectedValues) {
            Syntax::Token *token = nullptr;
            REQUIRE(lexer.hasNext());
            lexer.next(token);
            REQUIRE(token != nullptr);
            REQUIRE(token->type() == Syntax::TokenType::ConstVal); // Changed from TokenType::Real
            REQUIRE(token->value() != nullptr);
            REQUIRE(token->value()->type() == TjsValueType::Real);
            REQUIRE(token->value()->toReal() == Catch::Approx(expected));
        }
    }

    SECTION("字符串字面量") {
        sourceFile.load(r, R"("hello" "world" "test\nstring")");
        Syntax::Lexer lexer{ sourceFile };

        std::vector<std::string> expectedValues = { "hello", "world", "test\nstring" };

        for(const auto &expected : expectedValues) {
            Syntax::Token *token = nullptr;
            REQUIRE(lexer.hasNext());
            lexer.next(token);
            REQUIRE(token != nullptr);
            REQUIRE(token->value() != nullptr);
            REQUIRE(token->value()->type() == TjsValueType::String);
            REQUIRE(*token->value()->toString() == expected);
        }
    }
}

TEST_CASE("词法分析器 - 运算符识别") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("算术运算符") {
        sourceFile.load(r, "+ - * / %");
        Syntax::Lexer lexer{ sourceFile };

        std::vector<std::string> expectedOperators = { "+", "-", "*", "/", "%" };

        for(const auto &expected : expectedOperators) {
            Syntax::Token *token = nullptr;
            REQUIRE(lexer.hasNext());
            lexer.next(token);
            REQUIRE(token != nullptr);
            REQUIRE(token->name() == expected);
        }
    }

    SECTION("比较运算符") {
        sourceFile.load(r, "== != < <= > >=");
        Syntax::Lexer lexer{ sourceFile };

        std::vector<std::string> expectedOperators = { "==", "!=", "<", "<=", ">", ">=" };

        for(const auto &expected : expectedOperators) {
            Syntax::Token *token = nullptr;
            REQUIRE(lexer.hasNext());
            lexer.next(token);
            REQUIRE(token != nullptr);
            REQUIRE(token->name() == expected);
        }
    }

    SECTION("逻辑运算符") {
        sourceFile.load(r, "&& || !");
        Syntax::Lexer lexer{ sourceFile };

        std::vector<std::string> expectedOperators = { "&&", "||", "!" };

        for(const auto &expected : expectedOperators) {
            Syntax::Token *token = nullptr;
            REQUIRE(lexer.hasNext());
            lexer.next(token);
            REQUIRE(token != nullptr);
            REQUIRE(token->name() == expected);
        }
    }
}

TEST_CASE("词法分析器 - 分隔符识别") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("常用分隔符") {
        sourceFile.load(r, "() {} [] , ; .");
        Syntax::Lexer lexer{ sourceFile };

        std::vector<std::string> expectedDelimiters = { "(", ")", "{", "}", "[", "]", ",", ";", "." };

        for(const auto &expected : expectedDelimiters) {
            Syntax::Token *token = nullptr;
            REQUIRE(lexer.hasNext());
            lexer.next(token);
            REQUIRE(token != nullptr);
            REQUIRE(std::string(token->name()) == expected);
        }
    }
}

TEST_CASE("词法分析器 - 位置信息") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("行号和列号") {
        sourceFile.load(r, "line1\nline2\nline3");
        Syntax::Lexer lexer{ sourceFile };

        Syntax::Token *token = nullptr;
        lexer.next(token);
        REQUIRE(token->location.start().line == 0);
        REQUIRE(token->location.start().column == 0);

        lexer.next(token);
        REQUIRE(token->location.start().line == 1);
        REQUIRE(token->location.start().column == 0);

        lexer.next(token);
        REQUIRE(token->location.start().line == 2);
        REQUIRE(token->location.start().column == 0);
    }
}

TEST_CASE("词法分析器 - 错误处理") {
    Common::SourceFile sourceFile{};
    Common::Result r{};

    SECTION("无效字符") {
        sourceFile.load(r, "@ # $");
        Syntax::Lexer lexer{ sourceFile };

        // 应该能识别出无效token
        Syntax::Token *token = nullptr;
        while(lexer.hasNext()) {
            lexer.next(token);
            if(token->type() == Syntax::TokenType::Invalid) {
                REQUIRE(token->location.start().line == 0);
                REQUIRE(token->location.start().column >= 0);
            }
        }
    }
}