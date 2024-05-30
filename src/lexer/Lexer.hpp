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

#include <cassert>
#include <cstdint>
#include <functional>
#include <map>

#include "../common/SourceFile.hpp"
#include "../common/Result.hpp"
#include "Token.hpp"

namespace Ciallang::Syntax {
    using namespace std;

    static int8_t getHexNum(const char c) noexcept {
        if(c >= 'a' && c <= 'f') return static_cast<int8_t>(c - 'a' + 10);
        if(c >= 'A' && c <= 'F') return static_cast<int8_t>(c - 'A' + 10);
        if(c >= '0' && c <= '9') return static_cast<int8_t>(c - '0');
        return -1;
    }

    static int8_t getOctNum(const char c) noexcept {
        if(c >= '0' && c <= '7') return static_cast<int8_t>(c - '0');
        return -1;
    }

    static int8_t getBinNum(const char c) noexcept {
        if(c == '0' || c == '1') return static_cast<int8_t>(c - '0');
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

    class Lexer {
    public:
        using LexerCaseCallable = function<bool(Lexer*, Token*&)>;

        explicit Lexer(SourceFile&);

        bool next(Token*&);

        void skipComment();

        bool tackOverToken(Token& token) {
            assert(!_tokens.empty());

            token = *_tokens.front();

            if(token.type() == TokenType::EndOfFile) return false;

            delete _tokens.front();
            _tokens.erase(_tokens.begin());
            return true;
        }

        [[nodiscard]] bool hasNext() const;

        [[nodiscard]] constexpr const std::vector<Token*>& tokens() const {
            return _tokens;
        }

        [[nodiscard]] const Result& result() const;

        ~Lexer() noexcept {
            for(const auto& item : _tokens)
                delete item;
        }

    private:
        static std::multimap<int32_t, LexerCaseCallable> S_Cases;

        [[maybe_unused]] static void* S_LoadCases;

        static std::unordered_map<std::string, const Token&> S_Keywords;

        std::vector<Token*> _tokens{};
        bool _hasNext = true;
        Result _result{};
        SourceFile& _sourceFile;

        Token* makeToken(Token t) {
            auto* token = new Token{ std::move(t) };
            _tokens.push_back(token);
            return token;
        }

        Token* makeToken(const TokenType type, TjsValue&& value) {
            auto* token = new Token{ type, std::move(value) };
            _tokens.push_back(token);
            return token;
        }

        using OperatorTokenSet = std::vector<std::pair<const char*, const Token&>>;

        bool boringMatch(Token*&, const OperatorTokenSet& signMap);

        void rewindOneChar() const;

        string readIdentifier();

        bool lineTerminator(Token*&);

        void setTokenLocation(Token*&) const;

        [[nodiscard]] pair<uint32_t, uint32_t> getCurrentRowCol() const;

        bool match(const string& literal);

        // <
        bool gtSign(Token*&);

        bool octetLiteral(Token*&);

        // >
        bool ltSign(Token*&);

        bool exclamationSign(Token*&);

        bool ampersandSign(Token*&);

        bool vertLineSign(Token*&);

        bool dotSign(Token*&);

        bool slash(Token*&);

        bool backslash(Token*&);

        bool percent(Token*&);

        bool chevron(Token*&);

        bool singletonSign(Token*&);

        // =
        bool equalSign(Token*&);

        bool lineComment(Token*&);

        bool blockComment(Token*&);

        bool numberConstVal(Token*&);

        bool stringConstVal(Token*&);

        bool templateStringConstVal(Token*&);

        bool parseNonDecimalNumber(Token*&, stringstream&,
                                   int8_t (*)(char), int8_t);

        bool parseNonDecimalInteger(Token*&, const string&,
                                    int8_t (*)(char), int8_t);

        void parseNonDecimalReal(Token*&, const string&,
                                 int8_t (*)(char), int8_t);

        void extractNumber(int8_t (*)(char), const string& expMark, stringstream&, bool&);

        bool identifier(Token*&);

        bool plus(Token*&);

        bool minus(Token*&);

        bool mul(Token*&);

        int32_t read(bool skipWhitespace = true);

        StringParseState internalStringParser(
            Token*& token, char delimiter,
            bool* templateOver = nullptr,
            bool templateMode = false
        );
    };
}
