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

namespace cial::Syntax {

    class Lexer {
    public:
        using LexerCaseCallable = std::function<bool(Lexer *, Token *&)>;

        explicit Lexer(SourceFile &sourceFile, PreProcessor &preProcessor);

        bool next(Token *&token, bool enablePreProcessor = true);

        void skipComment();

        bool takeOverToken(Token &token);

        [[nodiscard]] bool hasNext() const;

        [[nodiscard]] size_t tokenSize() const { return _tokens.size(); }

        void peekToken(Token *&token) const { token = _tokens.front(); }

        ~Lexer() noexcept {
            for(const auto &item : _tokens)
                delete item;
        }

    private:
        // We don't need result return error; just for source file
        // We use Token to report error
        Result _r{};
        SourceFile &_sourceFile;
        PreProcessor &_preProcessor;

        std::deque<Token *> _tokens{};
        bool _hasNext{ true };

        template <typename... Args>
        Token *makeToken(Args &&...args) {
            _tokens.emplace_back(new Token{ std::forward<Args>(args)... });
            return _tokens.back();
        }

        void rewindOneChar() const;

        String readIdentifier();

        bool match(const String &literal);

        int32_t read(bool skipWhitespace = true);

        static void patchTokenLoc(Token &token, const std::pair<uint32_t, uint32_t> &start,
                                  const std::pair<uint32_t, uint32_t> &end) {
            token.location.start(start.first, start.second);
            token.location.start(end.first, end.second);
        }

        [[nodiscard]] std::pair<uint32_t, uint32_t> getRowCol(const size_t pos) const {
            return std::make_pair(_sourceFile.lineByIndex(pos)->line, _sourceFile.columnByIndex(pos));
        }

        bool readPreProcessorExpr(std::string &out);

        bool processor();

        bool matchOperator(Token *&token);

        bool octetLiteral(Token *&token);

        bool lineComment(Token *&token);

        bool blockComment(Token *&token);

        bool numberConstVal(Token *&token);

        bool stringConstVal(Token *&token);

        bool templateStringConstVal(Token *&token);

        bool parseNonDecimalNumber(Token *&token, std::stringstream &, std::int8_t (*)(char), std::int8_t);

        bool parseNonDecimalInteger(Token *&token, const std::string &, std::int8_t (*)(char), std::int8_t);

        void parseNonDecimalReal(Token *&token, const std::string &, std::int8_t (*)(char), std::int8_t);

        void extractNumber(std::int8_t (*)(char), const std::string &expMark, std::stringstream &, bool &);

        void identifier(Token *&);

        struct TemplateStringContext {
            enum class Stage {
                Init, // 刚开始，准备吐出第一个 '('
                Text, // 解析普通文字部分
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
        };

        TemplateStringContext tmplStrCtx;

        TemplateStringContext::State parseStringConstVal(Token *&token, char delimiter);


        static std::int8_t getHexNum(const char c) noexcept {
            if(c >= 'a' && c <= 'f')
                return static_cast<std::int8_t>(c - 'a' + 10);
            if(c >= 'A' && c <= 'F')
                return static_cast<std::int8_t>(c - 'A' + 10);
            if(c >= '0' && c <= '9')
                return static_cast<std::int8_t>(c - '0');
            return -1;
        }

        static std::int8_t getOctNum(const char c) noexcept {
            if(c >= '0' && c <= '7')
                return static_cast<std::int8_t>(c - '0');
            return -1;
        }

        static std::int8_t getBinNum(const char c) noexcept {
            if(c == '0' || c == '1')
                return static_cast<std::int8_t>(c - '0');
            return -1;
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
} // namespace cial::Syntax
