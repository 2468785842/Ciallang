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

#pragma once

#include "PreProcessor.hpp"
#include "Token.hpp"
#include "common/SourceFile.hpp"
#include "runtime/Runtime.hpp"

namespace cial::syntax {
    struct TemplateStringContext {
        enum class Stage {
            Init,       // 刚开始，准备吐出第一个 '('
            Text,       // 解析普通文字部分
            ExprOpener, // 准备吐出表达式开始的 '('
            Expression, // 正在解析表达式内部的 Token
            ExprCloser, // 准备吐出表达式结束的 ')'
            PlusAfterText,
            PlusAfterExpr,
            Terminator, // 准备吐出最后一个 ')'
        };

        enum class State { None, Delimiter, Ampersand, Dollar };

        bool active = false;
        Stage stage = Stage::Init;
        State parseState = State::None;
        char delimiter = -1;

        void reset() {
            active = false;
            stage = Stage::Init;
            parseState = State::None;
            delimiter = -1;
        }

        // 将转义符替换为ASCII
        static char unescapeBackSlash(const char c) noexcept {
            // convert "\?"
            // c must indicate "?"
            switch(c) {
                case 'a':
                    return 0x7;
                case 'b':
                    return 0x8;
                case 'f':
                    return 0xc;
                case 'n':
                    return 0xa;
                case 'r':
                    return 0xd;
                case 't':
                    return 0x9;
                case 'v':
                    return 0xb;
                default:
                    return c;
            }
        }
    };

    class Lexer {
    public:
        using LexerCaseCallable = std::function<bool(Lexer *, Token * &)>;

        explicit Lexer(SourceFile& sourceFile, PreProcessor& preProcessor);

        bool next(Token*& token, bool enablePreProcessor = true);

        void skipComment();

        bool takeOverToken(Token& token);

        [[nodiscard]] bool hasNext() const;

        [[nodiscard]] bool incomplete() const noexcept { return _incomplete; }

        [[nodiscard]] size_t tokenSize() const { return _tokens.size(); }

        void peekToken(Token*& token) const { token = _tokens.front(); }

        // 第 count 个待消费 token（1 基，需先 lookAhead 预填充）
        void peekTokenAt(Token*& token, const size_t count) const { token = _tokens[count - 1]; }

        ~Lexer() noexcept {
            for(const auto& item : _tokens)
                delete item;
        }

    private:
        // We don't need result return error; just for source file
        // We use Token to report error
        Result _r{};
        SourceFile& _sourceFile;
        PreProcessor& _preProcessor;

        std::deque<Token*> _tokens{};
        bool _hasNext{ true };
        bool _incomplete{};

        template <typename... Args>
        Token* makeToken(Args&&... args) {
            _tokens.emplace_back(new Token{ std::forward<Args>(args)... });
            return _tokens.back();
        }

        void rewindOneChar() const;

        String readIdentifier();

        bool match(const String& literal);

        i32 read(bool skipWhitespace = true);

        // 统一按字节半开区间落位：span 为权威，location 由行索引换算。
        // 所有分支必须记录扫描起点与消费后的游标，禁止 pos()-1 推算
        void patchTokenLoc(Token& token, size_t beginByte, size_t endByte) const;

        // 合成 token（模板字符串拼装的括号/加号）：零宽插入点，不指向源码词素
        void patchSyntheticToken(Token& token) const;

        // 错误 token 携带稳定诊断码与纯文本消息，由 Parser 单一上报
        Token* makeErrorToken(Token*& token, const char* code, const String& message);

        bool readPreProcessorExpr(std::string& out);

        bool processor();

        bool matchOperator(Token*& token);

        bool octetLiteral(Token*& token);

        bool lineComment(Token*& token);

        bool blockComment(Token*& token);

        bool numberConstVal(Token*& token);

        bool stringConstVal(Token*& token);

        bool regexConstVal(Token*& token);

        [[nodiscard]] bool isRegexStart() const;

        // 最近一次产出的有效 token 类型（正则字面量判据用；注释不更新）
        TokenType _lastTokenType{ TokenType::None };

        bool templateStringConstVal(Token*& token);

        bool parseNonDecimalNumber(Token*& token, std::stringstream&, i8 (*)(char), i8);

        bool parseNonDecimalInteger(Token*& token, const std::string&, i8 (*)(char), i8);

        void parseNonDecimalReal(Token*& token, const std::string&, i8 (*)(char), i8);

        void extractNumber(i8 (*)(char), const std::string& expMark, std::stringstream&, bool&);

        void identifier(Token*&);

        TemplateStringContext tmplStrCtx;

        TemplateStringContext::State parseStringConstVal(Token*& token, char delimiter);

        static i8 getHexNum(const char c) noexcept {
            if(c >= 'a' && c <= 'f')
                return static_cast<i8>(c - 'a' + 10);
            if(c >= 'A' && c <= 'F')
                return static_cast<i8>(c - 'A' + 10);
            if(c >= '0' && c <= '9')
                return static_cast<i8>(c - '0');
            return -1;
        }

        static i8 getOctNum(const char c) noexcept {
            if(c >= '0' && c <= '7')
                return static_cast<i8>(c - '0');
            return -1;
        }

        static i8 getBinNum(const char c) noexcept {
            if(c == '0' || c == '1')
                return static_cast<i8>(c - '0');
            return -1;
        }
    };
} // namespace cial::syntax
