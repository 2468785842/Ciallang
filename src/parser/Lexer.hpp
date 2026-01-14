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
#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "runtime/Runtime.hpp"
// #include "PreProcessor.hpp"

namespace cial::Syntax {
    class LexemeGuard {
    public:
        explicit LexemeGuard(SourceFile &src) : _src(src) {
            // save mark
            _src.pushMark();
            const auto [startColumn, startLine] = getCurrentRowCol();
            _startCol = startColumn;
            _startLine = startLine;
        }

        bool commit(Token &tok, const bool ok = true) const {
            const auto [endCol, endLine] = getCurrentRowCol();
            tok.location.start(_startLine, _startCol);
            tok.location.end(endLine, endCol);
            return ok;
        }

        void restoreMark() const { _src.restoreTopMark(); }

        ~LexemeGuard() { _src.popMark(); }

    private:
        SourceFile &_src;
        uint32_t _startLine, _startCol;

        [[nodiscard]] std::pair<uint32_t, uint32_t> getCurrentRowCol() const {
            return std::make_pair(_src.columnByIndex(_src.pos()), _src.lineByIndex(_src.pos())->line);
        }
    };

    class Lexer {
    public:
        using LexerCaseCallable = std::function<bool(Lexer *, Token *&)>;

        explicit Lexer(SourceFile &sourceFile, PreProcessor &preProcessor);

        bool next(Result &r, Token *&token);

        void skipComment(Result &r);

        bool takeOverToken(Token &token);

        [[nodiscard]] bool hasNext() const;

        [[nodiscard]] size_t tokenSize() const { return _tokens.size(); }

        void peekToken(Token *&token) const { token = _tokens.front(); }

        ~Lexer() noexcept {
            for(const auto &item : _tokens)
                delete item;
        }

    private:
        SourceFile &_sourceFile;
        PreProcessor &_preProcessor;

        std::deque<Token *> _tokens{};
        bool _hasNext{ true };
        bool _disablePreProcess{ false };

        template <typename... Args>
        Token *makeToken(Args &&...args) {
            _tokens.emplace_back(new Token{ std::forward<Args>(args)... });
            return _tokens.back();
        }

        void rewindOneChar() const;

        String readIdentifier(Result &r) const;

        bool match(Result &r, const String &literal) const;

        int32_t read(Result &r, bool skipWhitespace = true) const;

        bool readParenExpr(Result &r, std::string &out) const;

        bool processor(Result &r) const;

        bool lineTerminator(Result &r, Token *&token);

        bool matchOperator(Token *&token);

        bool octetLiteral(Result &r, Token *&token);

        bool lineComment(Result &r, Token *&token);

        bool blockComment(Result &r, Token *&token);

        bool numberConstVal(Result &r, Token *&token);

        bool stringConstVal(Result &r, Token *&token);

        bool templateStringConstVal(Result &r, Token *&token);

        bool parseNonDecimalNumber(Result &r, Token *&token, std::stringstream &, std::int8_t (*)(char), std::int8_t);

        bool parseNonDecimalInteger(Result &r, Token *&token, const std::string &, std::int8_t (*)(char), std::int8_t);

        void parseNonDecimalReal(Result &r, Token *&token, const std::string &, std::int8_t (*)(char), std::int8_t);

        void extractNumber(Result &r, std::int8_t (*)(char), const std::string &expMark, std::stringstream &,
                           bool &) const;

        bool identifier(Result &r, Token *&);

        StringParseState internalStringParser(Result &r, Token *&token, char delimiter, bool *templateOver = nullptr,
                                              bool templateMode = false);


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
