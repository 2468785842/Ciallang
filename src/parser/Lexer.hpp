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

#include "Token.hpp"
#include "common/Result.hpp"
#include "common/SourceFile.hpp"
#include "runtime/Runtime.hpp"

namespace cial::Syntax {

    class Lexer {
    public:
        using LexerCaseCallable = std::function<bool(Lexer *, Token *&)>;

        explicit Lexer(SourceFile &sourceFile);

        bool next(Token *&token);

        void skipComment();

        bool takeOverToken(Token &token);

        [[nodiscard]] bool hasNext() const;

        [[nodiscard]] const Result &result() const;

        [[nodiscard]] size_t tokenSize() const { return _tokens.size(); }

        void peekToken(Token *&token) const { token = _tokens.front(); }

        ~Lexer() noexcept {
            for(const auto &item : _tokens)
                delete item;
        }

    private:
        static std::multimap<std::uint8_t, LexerCaseCallable> S_Cases;

        SourceFile &_sourceFile;

        std::deque<Token *> _tokens{};
        Result _result{};
        bool _hasNext = true;

        template <typename... Args>
        Token *makeToken(Args &&...args) {
            _tokens.emplace_back(new Token{ std::forward<Args>(args)... });
            return _tokens.back();
        }

        void rewindOneChar() const;

        String readIdentifier();

        bool lineTerminator(Token *&);

        void setTokenLocation(Token *&) const;

        [[nodiscard]] std::pair<uint32_t, uint32_t> getCurrentRowCol() const;

        bool match(const String &literal);

        bool matchOperator(Token *&token);

        bool octetLiteral(Token *&);

        bool lineComment(Token *&);

        bool blockComment(Token *&);

        bool numberConstVal(Token *&);

        bool stringConstVal(Token *&);

        bool templateStringConstVal(Token *&);

        bool parseNonDecimalNumber(Token *&, std::stringstream &, std::int8_t (*)(char), std::int8_t);

        bool parseNonDecimalInteger(Token *&, const std::string &, std::int8_t (*)(char), std::int8_t);

        void parseNonDecimalReal(Token *&, const std::string &, std::int8_t (*)(char), std::int8_t);

        void extractNumber(std::int8_t (*)(char), const std::string &expMark, std::stringstream &, bool &);

        bool identifier(Token *&);

        int32_t read(bool skipWhitespace = true);

        StringParseState internalStringParser(Token *&token, char delimiter, bool *templateOver = nullptr,
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
