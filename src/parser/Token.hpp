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

#include <frozen/string.h>
#include <frozen/unordered_map.h>

#include <utility>

#include "common/SourceLocation.hpp"
#include "types/Octet.hpp"
#include "types/Value.hpp"

namespace cial::syntax {
    using namespace Common;
#define KEYWORD_LIST(X)                                                                                                \
    X(Void, "void")                                                                                                    \
    X(Synchronized, "synchronized")                                                                                    \
    X(Goto, "goto")                                                                                                    \
    X(With, "with")                                                                                                    \
    X(Null, "null")                                                                                                    \
    X(Import, "import")                                                                                                \
    X(Export, "export")                                                                                                \
    X(Throw, "throw")                                                                                                  \
    X(Try, "try")                                                                                                      \
    X(Catch, "catch")                                                                                                  \
    X(Finally, "finally")                                                                                              \
    X(Debugger, "debugger")                                                                                            \
    X(Property, "property")                                                                                            \
    X(Setter, "setter")                                                                                                \
    X(Getter, "getter")                                                                                                \
    X(Function, "function")                                                                                            \
    X(Enum, "enum")                                                                                                    \
    X(Class, "class")                                                                                                  \
    X(This, "this")                                                                                                    \
    X(Super, "super")                                                                                                  \
    X(Extends, "extends")                                                                                              \
    X(Private, "private")                                                                                              \
    X(Public, "public")                                                                                                \
    X(Protected, "protected")                                                                                          \
    X(Static, "static")                                                                                                \
    X(Return, "return")                                                                                                \
    X(Var, "var")                                                                                                      \
    X(Const, "const")                                                                                                  \
    X(If, "if")                                                                                                        \
    X(Else, "else")                                                                                                    \
    X(Int, "int")                                                                                                      \
    X(Real, "real")                                                                                                    \
    X(String, "string")                                                                                                \
    X(Octet, "octet")                                                                                                  \
    X(New, "new")                                                                                                      \
    X(Do, "do")                                                                                                        \
    X(While, "while")                                                                                                  \
    X(For, "for")                                                                                                      \
    X(Continue, "continue")                                                                                            \
    X(Break, "break")                                                                                                  \
    X(Switch, "switch")                                                                                                \
    X(Case, "case")                                                                                                    \
    X(Default, "default")                                                                                              \
    X(Delete, "delete")                                                                                                \
    X(In, "in")                                                                                                        \
    X(InContextOf, "incontextof")                                                                                      \
    X(Typeof, "typeof")                                                                                                \
    X(Isvalid, "isvalid")                                                                                              \
    X(Invalidate, "invalidate")                                                                                        \
    X(Instanceof, "instanceof")                                                                                        \
    X(Global, "global")

#define SYMBOL_LIST(X)                                                                                                 \
    X(Omit, "...")                                                                                                     \
    X(Comma, ",")                                                                                                      \
    X(Assignment, "=")                                                                                                 \
    X(AmpersandEqual, "&=")                                                                                            \
    X(VertLineEqual, "|=")                                                                                             \
    X(ChevronEqual, "^=")                                                                                              \
    X(MinusEqual, "-=")                                                                                                \
    X(PlusEqual, "+=")                                                                                                 \
    X(PercentEqual, "%=")                                                                                              \
    X(SlashEqual, "/=")                                                                                                \
    X(BackslashEqual, "\\=")                                                                                           \
    X(AsteriskEqual, "*=")                                                                                             \
    X(LogicalOrEqual, "||=")                                                                                           \
    X(LogicalAndEqual, "&&=")                                                                                          \
    X(RBitShiftEqual, ">>>=")                                                                                          \
    X(LArithShiftEqual, "<<=")                                                                                         \
    X(RArithShiftEqual, ">>=")                                                                                         \
    X(Question, "?")                                                                                                   \
    X(LogicalOr, "||")                                                                                                 \
    X(LogicalAnd, "&&")                                                                                                \
    X(VertLine, "|")                                                                                                   \
    X(Chevron, "^")                                                                                                    \
    X(Ampersand, "&")                                                                                                  \
    X(NotEqual, "!=")                                                                                                  \
    X(Equal, "==")                                                                                                     \
    X(DiscNotEqual, "!==")                                                                                             \
    X(DiscEqual, "===")                                                                                                \
    X(Swap, "<->")                                                                                                     \
    X(Lt, "<")                                                                                                         \
    X(Gt, ">")                                                                                                         \
    X(LtOrEqual, "<=")                                                                                                 \
    X(GtOrEqual, ">=")                                                                                                 \
    X(RArithShift, ">>")                                                                                               \
    X(LArithShift, "<<")                                                                                               \
    X(RBitShift, ">>>")                                                                                                \
    X(Percent, "%")                                                                                                    \
    X(Slash, "/")                                                                                                      \
    X(Backslash, "\\")                                                                                                 \
    X(Asterisk, "*")                                                                                                   \
    X(Exclamation, "!")                                                                                                \
    X(Tilde, "~")                                                                                                      \
    X(Decrement, "--")                                                                                                 \
    X(Increment, "++")                                                                                                 \
    X(Plus, "+")                                                                                                       \
    X(Minus, "-")                                                                                                      \
    X(Sharp, "#")                                                                                                      \
    X(Dollar, "$")                                                                                                     \
    X(LParenthesis, "(")                                                                                               \
    X(Dot, ".")                                                                                                        \
    X(LBracket, "[")                                                                                                   \
    X(RBracket, "]")                                                                                                   \
    X(RParenthesis, ")")                                                                                               \
    X(Colon, ":")                                                                                                      \
    X(SemiColon, ";")                                                                                                  \
    X(LeftCurlyBrace, "{")                                                                                             \
    X(RightCurlyBrace, "}")

#define TOKEN_SYMBOL_LIST(X)                                                                                           \
    X(Error, "error")                                                                                                  \
    X(Invalid, "invalid")                                                                                              \
    X(EndOfFile, "end_of_file")                                                                                        \
    X(LineComment, "line_comment")                                                                                     \
    X(BlockComment, "block_comment")                                                                                   \
    X(Identifier, "identifier")                                                                                        \
    X(ConstVal, "const_val")

#define CONSTANT_VAL_LIST(X)                                                                                           \
    X(ConstVal, "true", 1)                                                                                             \
    X(ConstVal, "false", 0)                                                                                            \
    X(ConstVal, "Infinity", Real::negativeInf())                                                                       \
    X(ConstVal, "NaN", Real::signalingNan())

    enum class TokenType {
        None,
#define LIST_TO_ENUM(NAME, IGNORE) NAME,
        SYMBOL_LIST(LIST_TO_ENUM) KEYWORD_LIST(LIST_TO_ENUM) TOKEN_SYMBOL_LIST(LIST_TO_ENUM)
#undef LIST_TO_ENUM
    };

    static constexpr auto S_TypeToName = frozen::make_unordered_map<TokenType, frozen::string>({
#define LIST_TO_NAME_PAIR(NAME, SYMBOL) { TokenType::NAME, SYMBOL },
        SYMBOL_LIST(LIST_TO_NAME_PAIR) KEYWORD_LIST(LIST_TO_NAME_PAIR) TOKEN_SYMBOL_LIST(LIST_TO_NAME_PAIR)
#undef LIST_TO_NAME_PAIR
    });

    static constexpr const char *tokenTypeToStr(const TokenType type) {
        if(const auto it = S_TypeToName.find(type); it != S_TypeToName.end()) {
            return it->second.data();
        }
        return "unknown";
    }

    enum class TokenValueType : u8 { None, Integer, Real, String, Octet };

    struct Token {
        SourceLocation location{};

        constexpr explicit Token() = default;

        constexpr explicit Token(const TokenType type) : _type(type) {}

        explicit Token(const TokenType type, const Integer value) :
            _type(type), _valueType(TokenValueType::Integer), _integer(value) {}

        explicit Token(const TokenType type, const Real value) :
            _type(type), _valueType(TokenValueType::Real), _real(value) {}

        explicit Token(const TokenType type, String string) : _type(type), _valueType(TokenValueType::String) {
            _string = new String{ std::move(string) };
        }

        explicit Token(const TokenType type, Octet octet) : _type(type), _valueType(TokenValueType::Octet) {
            _octet = new Octet{ std::move(octet) };
        }

        Token(Token &&token) noexcept {
            _type = token._type;
            _valueType = token._valueType;
            location = token.location;

            switch(_valueType) {
                case TokenValueType::None:
                    break;
                case TokenValueType::Integer:
                    _integer = token._integer;
                    break;
                case TokenValueType::Real:
                    _real = token._real;
                    break;
                case TokenValueType::String:
                    _string = token._string;
                    token._string = nullptr;
                    break;
                case TokenValueType::Octet:
                    _octet = token._octet;
                    token._octet = nullptr;
                    break;
            }
            token._type = TokenType::None;
            token._valueType = TokenValueType::None;
        }

        Token &operator=(Token &&token) noexcept {
            if(this != &token) {
                this->~Token();
                new(this) Token(std::move(token));
            }
            return *this;
        }


        Token(const Token &token) {
            _type = token._type;
            _valueType = token._valueType;
            location = token.location;

            switch(_valueType) {
                case TokenValueType::None:
                    break;
                case TokenValueType::Integer:
                    _integer = token._integer;
                    break;
                case TokenValueType::Real:
                    _real = token._real;
                    break;
                case TokenValueType::String:
                    _string = new String{ *token._string };
                    break;
                case TokenValueType::Octet:
                    _octet = new Octet{ *token._octet };
                    break;
            }
        }

        Token &operator=(const Token &token) {
            if(this != &token) {
                this->~Token();
                new(this) Token(token);
            }
            return *this;
        }

        bool operator==(const TokenType tokenType) const { return _type == tokenType; }

        [[nodiscard]] constexpr TokenType type() const noexcept { return _type; }

        [[nodiscard]] constexpr TokenValueType valueType() const noexcept { return _valueType; }

        [[nodiscard]] constexpr Integer getInteger() const noexcept { return _integer; }

        [[nodiscard]] constexpr Real getReal() const noexcept { return _real; }

        [[nodiscard]] const String &getString() const noexcept { return *_string; }

        [[nodiscard]] const Octet &getOctet() const noexcept { return *_octet; }

        [[nodiscard]] constexpr const char *name() const noexcept { return tokenTypeToStr(_type); }

        ~Token() noexcept {
            if(_valueType == TokenValueType::String) {
                delete _string;
            } else if(_valueType == TokenValueType::Octet) {
                delete _octet;
            }
        }

    private:
        TokenType _type{ TokenType::None };
        TokenValueType _valueType{ TokenValueType::None };

        union {
            Integer _integer{};
            Real _real;
            String *_string;
            Octet *_octet;
        };
    };

    class AssignToken {
    public:
        static constexpr bool support(const TokenType tokenType) noexcept {
            return _assignToNonAssign.find(tokenType) != _assignToNonAssign.end();
        }

        static constexpr TokenType strip(const TokenType tokenType) noexcept {
            if(const auto it = _assignToNonAssign.find(tokenType); it != _assignToNonAssign.end()) {
                return it->second;
            }

            return tokenType;
        }

    private:
        static constexpr auto _assignToNonAssign = frozen::make_unordered_map<TokenType, TokenType>({
            { TokenType::PlusEqual, TokenType::Plus }, // +=
            { TokenType::MinusEqual, TokenType::Minus }, // -=
            { TokenType::SlashEqual, TokenType::Slash }, // /=
            { TokenType::PercentEqual, TokenType::Percent }, // %=
            { TokenType::AsteriskEqual, TokenType::Asterisk }, // *=
            { TokenType::VertLineEqual, TokenType::VertLine }, // |=
            { TokenType::AmpersandEqual, TokenType::Ampersand }, // &=
            { TokenType::ChevronEqual, TokenType::Chevron }, // ^=
            { TokenType::BackslashEqual, TokenType::Backslash }, // \=
            { TokenType::LogicalOrEqual, TokenType::LogicalOr }, // ||=
            { TokenType::LogicalAndEqual, TokenType::LogicalAnd }, // &&=
            { TokenType::RBitShiftEqual, TokenType::RBitShift }, // >>>=
            { TokenType::LArithShiftEqual, TokenType::LArithShift }, // <<=
            { TokenType::RArithShiftEqual, TokenType::RArithShift } // >>=
        });
    };
} // namespace cial::syntax
