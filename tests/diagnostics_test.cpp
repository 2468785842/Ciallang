//
// 诊断与源码位置回归：字节半开区间、安全行索引、词法/语法诊断的
// 单一上报与位置正确性。验收对应 doc/diagnostics-plan.md 批次 1–3。
//

#include <catch2/catch_all.hpp>

#include "parser/Lexer.hpp"
#include "parser/Parser.hpp"

using namespace cial;

namespace {
    // 收集全部 token（忽略注释不适用此处：需逐类断言范围）
    struct LexResult {
        Vec<syntax::Token> tokens;
        bool lexerFailed = false;
    };

    LexResult lexAll(const std::string& source) {
        Common::SourceFile file;
        Common::Result diagnostics;
        REQUIRE(file.load(diagnostics, source));
        REQUIRE_FALSE(diagnostics.isFailed());

        PreProcessor preProcessor;
        syntax::Lexer lexer{ file, preProcessor };
        LexResult result;
        syntax::Token* token{};
        while(true) {
            const bool ok = lexer.next(token, true);
            result.tokens.push_back(std::move(*token));
            if(!ok)
                result.lexerFailed = true;
            if(result.tokens.back().type() == syntax::TokenType::EndOfFile)
                break;
        }
        return result;
    }

    const syntax::Token& tokenAt(const LexResult& result, const size_t index) { return result.tokens[index]; }

    // 编译并返回首条诊断（若有）
    Opt<Common::ResultMessage> firstDiagnostic(const std::string& source) {
        Common::SourceFile file;
        file.setSourceName("test.tjs");
        Common::Result diagnostics;
        if(!file.load(diagnostics, source))
            return diagnostics.messages().empty() ? Opt<Common::ResultMessage>{} : diagnostics.messages().front();
        if(diagnostics.isFailed())
            return diagnostics.messages().front();
        PreProcessor preProcessor;
        syntax::Parser parser{ file, preProcessor };
        parser.parse(diagnostics);
        if(diagnostics.isFailed())
            return diagnostics.messages().front();
        return {};
    }
} // namespace

TEST_CASE (
"source span semantics"
)
 {
    SECTION("unknown span is distinct from zero-width span at zero") {
        REQUIRE_FALSE(Common::SourceSpan{}.valid());
        const Common::SourceSpan zero{ 0, 0 };
        REQUIRE(zero.valid());
        REQUIRE(zero.empty());
        REQUIRE_FALSE(zero == Common::SourceSpan{});
    }

    SECTION("span clamping keeps bounded ranges") {
        Common::SourceFile file;
        Common::Result diagnostics;
        REQUIRE(file.load(diagnostics, "ab"));
        REQUIRE(file.spanOf(1, 2) == Common::SourceSpan{ 1, 2 });
        REQUIRE_FALSE(file.spanOf(2, 1).valid());  // 反向区间无效
        REQUIRE_FALSE(file.spanOf(0, 3).valid());  // 越界无效
        REQUIRE(file.eofSpan() == Common::SourceSpan{ 2, 2 });
    }
}

TEST_CASE (
"line index handles line-ending and empty edge cases"
)
 {
    Common::SourceFile file;
    Common::Result diagnostics;

    SECTION("empty buffer still has a queryable line") {
        REQUIRE(file.load(diagnostics, ""));
        REQUIRE(file.numberOfLines() == 1);
        const auto location = file.locationByIndex(0);
        REQUIRE(location.start().line == 0);
        REQUIRE(location.start().column == 0);
    }

    SECTION("file ending with newline maps EOF to the empty final line") {
        REQUIRE(file.load(diagnostics, "if(a){\n"));
        const auto location = file.locationByIndex(7); // EOF 偏移
        REQUIRE(location.start().line == 1);
        REQUIRE(location.start().column == 0);
    }

    SECTION("file without trailing newline") {
        REQUIRE(file.load(diagnostics, "var a;"));
        const auto location = file.locationByIndex(6);
        REQUIRE(location.start().line == 0);
        REQUIRE(location.start().column == 6);
    }

    SECTION("CRLF columns exclude the carriage return") {
        REQUIRE(file.load(diagnostics, "var a;\r\nvar b;\r\n"));
        const auto second = file.locationByIndex(8); // 'v' of 第二行
        REQUIRE(second.start().line == 1);
        REQUIRE(second.start().column == 0);
        // 行内容不含 \r：substring 到行末为 "var b;"
        const auto *line = file.lineByNumber(1);
        REQUIRE(file.substring(line->begin, line->end) == "var b;");
    }

    SECTION("invalid utf-8 still yields bounded locations") {
        const std::string bad = "var a = \"\xff\";\nvar b;";
        REQUIRE_FALSE(file.load(diagnostics, bad)); // 校验失败
        REQUIRE(diagnostics.isFailed());
        const auto &message = diagnostics.messages().front();
        REQUIRE(message.code() == "lex.invalid-byte");
        REQUIRE(message.hasSpan());
        REQUIRE(message.span().begin == 9); // 指向非法字节本身
        // 行索引仍然可用（纯字节构建）
        const auto location = file.locationByIndex(17);
        REQUIRE(location.start().line == 1);
    }

    SECTION("NUL byte reports its position") {
        const std::string withNul{ "var a;\0var b;", 13 }; // 显式长度保留内嵌 NUL
        REQUIRE_FALSE(file.load(diagnostics, withNul));
        const auto &message = diagnostics.messages().front();
        REQUIRE(message.code() == "lex.invalid-byte");
        REQUIRE(message.span().begin == 6);
    }

    SECTION("source name override") {
        Common::SourceFile named;
        named.setSourceName("untitled:1");
        Common::Result r;
        REQUIRE(named.load(r, ";"));
        REQUIRE(named.sourceName() == "untitled:1");
    }
}

TEST_CASE (
"lexer records byte half-open spans per token class"
)
 {
    SECTION("ascii identifier and single-char operator are non-zero width") {
        const auto result = lexAll("var ab;");
        const auto &varToken = tokenAt(result, 0);
        REQUIRE(varToken.type() == syntax::TokenType::Var);
        REQUIRE(varToken.span == Common::SourceSpan{ 0, 3 });
        const auto &name = tokenAt(result, 1);
        REQUIRE(name.type() == syntax::TokenType::Identifier);
        REQUIRE(name.span == Common::SourceSpan{ 4, 6 });
        const auto &semi = tokenAt(result, 2);
        REQUIRE(semi.type() == syntax::TokenType::SemiColon);
        REQUIRE(semi.span == Common::SourceSpan{ 6, 7 }); // 不再是零宽
    }

    SECTION("multi-byte operator consumes full lexeme") {
        const auto result = lexAll("a >>= 1");
        const auto &shift = tokenAt(result, 1);
        REQUIRE(shift.type() == syntax::TokenType::RArithShiftEqual);
        REQUIRE(shift.span == Common::SourceSpan{ 2, 5 });
    }

    SECTION("cjk identifier boundaries land on code points") {
        const auto result = lexAll("中文;");
        const auto &name = tokenAt(result, 0);
        REQUIRE(name.type() == syntax::TokenType::Identifier);
        REQUIRE(name.span == Common::SourceSpan{ 0, 6 }); // 两个三字节字符
        const auto &semi = tokenAt(result, 1);
        REQUIRE(semi.span == Common::SourceSpan{ 6, 7 });
    }

    SECTION("string literal covers quotes") {
        const auto result = lexAll("\"a中\"");
        const auto &str = tokenAt(result, 0);
        REQUIRE(str.type() == syntax::TokenType::ConstVal);
        REQUIRE(str.span == Common::SourceSpan{ 0, 6 }); // 引号+a+中(3B)+引号
    }

    SECTION("eof token spans size..size") {
        const auto result = lexAll("a");
        const auto &eof = tokenAt(result, 1);
        REQUIRE(eof.type() == syntax::TokenType::EndOfFile);
        REQUIRE(eof.span == Common::SourceSpan{ 1, 1 });
    }

    SECTION("crlf keeps line/column of following tokens") {
        const auto result = lexAll("var a;\r\nvar b;");
        const auto &secondVar = tokenAt(result, 3);
        REQUIRE(secondVar.type() == syntax::TokenType::Var);
        REQUIRE(secondVar.span == Common::SourceSpan{ 8, 11 });
        REQUIRE(secondVar.location.start().line == 1);
        REQUIRE(secondVar.location.start().column == 0);
    }

    SECTION("line and block comments span their bodies") {
        const auto result = lexAll("// c\r\n/* 块 */\na;");
        REQUIRE(tokenAt(result, 0).type() == syntax::TokenType::LineComment);
        REQUIRE(tokenAt(result, 0).span == Common::SourceSpan{ 0, 6 }); // 含换行前内容（\r 剥离）
        REQUIRE(tokenAt(result, 1).type() == syntax::TokenType::BlockComment);
        REQUIRE(tokenAt(result, 1).span == Common::SourceSpan{ 6, 15 });
    }

    SECTION("regexp literal covers pattern and flags") {
        const auto result = lexAll("var re = /a中/gi;");
        const auto &re = tokenAt(result, 3);
        REQUIRE(re.type() == syntax::TokenType::RegExp);
        REQUIRE(re.span == Common::SourceSpan{ 9, 17 });
    }

    SECTION("number literal spans digits") {
        const auto result = lexAll("0x1F;");
        const auto &num = tokenAt(result, 0);
        REQUIRE(num.type() == syntax::TokenType::ConstVal);
        REQUIRE(num.span == Common::SourceSpan{ 0, 4 });
    }
}

TEST_CASE (
"lexer failure tokens carry code, span and eof relation"
)
 {
    SECTION("unterminated string anchors at opening quote and EOF") {
        Common::SourceFile file;
        Common::Result diagnostics;
        REQUIRE(file.load(diagnostics, "\"abc"));
        PreProcessor preProcessor;
        syntax::Lexer lexer{ file, preProcessor };
        syntax::Token *token{};
        REQUIRE_FALSE(lexer.next(token));
        REQUIRE(token->type() == syntax::TokenType::Error);
        REQUIRE(token->code == "lex.unterminated-string");
        REQUIRE(token->span == Common::SourceSpan{ 0, 4 }); // [引号, EOF)
        REQUIRE(lexer.incomplete()); // REPL 可继续输入
    }

    SECTION("unterminated block comment anchors at comment start") {
        Common::SourceFile file;
        Common::Result diagnostics;
        REQUIRE(file.load(diagnostics, "var a;\n/* 未闭合"));
        PreProcessor preProcessor;
        syntax::Lexer lexer{ file, preProcessor };
        syntax::Token *token{};
        lexer.next(token); // var
        lexer.next(token); // a
        lexer.next(token); // ;
        REQUIRE_FALSE(lexer.next(token));
        REQUIRE(token->type() == syntax::TokenType::Error);
        REQUIRE(token->code == "lex.unterminated-comment");
        REQUIRE(token->span.begin == 7);
        REQUIRE(token->span.end == file.length());
        REQUIRE(lexer.incomplete());
    }

    SECTION("unexpected ascii character points at the byte") {
        const auto result = lexAll("a ` b");
        REQUIRE(result.lexerFailed);
        const auto &bad = tokenAt(result, 1);
        REQUIRE(bad.type() == syntax::TokenType::Error);
        REQUIRE(bad.code == "lex.invalid-character");
        REQUIRE(bad.span == Common::SourceSpan{ 2, 3 });
    }

    SECTION("malformed preprocessor directive is not silently skipped") {
        const auto diagnostic = firstDiagnostic("var a = 1;\n@set\n");
        REQUIRE(diagnostic.has_value());
        REQUIRE((*diagnostic).code() == "lex.preprocessor");
        REQUIRE((*diagnostic).message().find("@set") != std::string::npos);
        REQUIRE((*diagnostic).span().begin == 11); // '@' 处
    }
}

TEST_CASE (
"parser reports first diagnostic with lexeme-accurate spans"
)
 {
    SECTION("missing expression points at the offending token") {
        const auto diagnostic = firstDiagnostic("var b = ;");
        REQUIRE(diagnostic.has_value());
        REQUIRE((*diagnostic).message().find("';'") != std::string::npos);
        REQUIRE((*diagnostic).span() == Common::SourceSpan{ 8, 9 });
        REQUIRE((*diagnostic).code() == "parse.invalid-expression");
    }

    SECTION("missing rhs does not cascade") {
        const auto diagnostic = firstDiagnostic("var b = 5 + ;");
        REQUIRE(diagnostic.has_value());
        // 首条诊断后不再补报 "binary operator expects..."
        Common::Result diagnostics;
        Common::SourceFile source;
        REQUIRE(source.load(diagnostics, "var b = 5 + ;"));
        PreProcessor preProcessor;
        syntax::Parser parser{ source, preProcessor };
        parser.parse(diagnostics);
        REQUIRE(diagnostics.messages().size() == 1);
    }

    SECTION("two independent errors both surface via sync") {
        Common::SourceFile source;
        Common::Result diagnostics;
        REQUIRE(source.load(diagnostics, "var ;\nvar ;\n"));
        // 声明缺名字：两条各报一次
        PreProcessor preProcessor;
        syntax::Parser parser{ source, preProcessor };
        parser.parse(diagnostics);
        REQUIRE(diagnostics.isFailed());
        REQUIRE(diagnostics.messages().size() >= 2);
    }

    SECTION("missing closing brace reports the item at EOF insertion point") {
        const auto diagnostic = firstDiagnostic("if(a>b){");
        REQUIRE(diagnostic.has_value());
        REQUIRE((*diagnostic).message().find("'}'") != std::string::npos);
        REQUIRE((*diagnostic).message().find("end of file") != std::string::npos);
        REQUIRE((*diagnostic).code() == "parse.expected-token");
        REQUIRE((*diagnostic).span() == Common::SourceSpan{ 8, 8 }); // EOF 零宽插入点
    }

    SECTION("unexpected token reports expected and actual") {
        const auto diagnostic = firstDiagnostic("if a { }");
        REQUIRE(diagnostic.has_value());
        const auto &message = (*diagnostic).message();
        REQUIRE(message.find("'('") != std::string::npos);
        REQUIRE(message.find("'a'") != std::string::npos);
    }

    SECTION("lexer error message is not replaced by generic prefix failure") {
        const auto diagnostic = firstDiagnostic("var s = \"abc");
        REQUIRE(diagnostic.has_value());
        REQUIRE((*diagnostic).message().find("string value need close") != std::string::npos);
        REQUIRE((*diagnostic).code() == "lex.unterminated-string");
    }

    SECTION("diagnostics carry source identity and excerpt without inline prefix") {
        const auto diagnostic = firstDiagnostic("var b = ;");
        REQUIRE((*diagnostic).source() == "test.tjs");
        REQUIRE((*diagnostic).message().find("@") == std::string::npos); // 无 (file@l:c) 前缀
        REQUIRE_FALSE((*diagnostic).details().empty());                  // 摘录已生成
    }

    SECTION("cjk prefix keeps byte-accurate span on the same line") {
        const auto diagnostic = firstDiagnostic("var a = \"中文\"; var b = ;");
        REQUIRE(diagnostic.has_value());
        // 'var a = "中文"; ' 共 18 字节（中文 6 字节），'var b = ;' 的 ';' 在 18+8
        REQUIRE((*diagnostic).span() == Common::SourceSpan{ 26, 27 });
        REQUIRE((*diagnostic).location().start().line == 0);
        REQUIRE((*diagnostic).location().start().column == 26);
    }
}
