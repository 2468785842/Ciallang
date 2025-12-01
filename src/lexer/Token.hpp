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
#include "types/Value.hpp"

namespace Ciallang::Syntax {
    using namespace Common;
#define TOKEN_SYMBOL_PAIR_ENUM(X)                                                                                      \
    X(Invalid, "invalid")                                                                                              \
    X(EndOfFile, "end_of_file")                                                                                        \
    X(Void, "void")                                                                                                    \
    X(Synchronized, "synchronized")                                                                                    \
    X(With, "with")                                                                                                    \
    X(Var, "var")                                                                                                      \
    X(Const, "const")                                                                                                  \
    X(Enum, "enum")                                                                                                    \
    X(Goto, "goto")                                                                                                    \
    X(Throw, "throw")                                                                                                  \
    X(Try, "try")                                                                                                      \
    X(Setter, "setter")                                                                                                \
    X(Getter, "getter")                                                                                                \
    X(Catch, "catch")                                                                                                  \
    X(Omit, "...")                                                                                                     \
    X(Continue, "continue")                                                                                            \
    X(Function, "function")                                                                                            \
    X(Debugger, "debugger")                                                                                            \
    X(Default, "default")                                                                                              \
    X(Case, "case")                                                                                                    \
    X(Extends, "extends")                                                                                              \
    X(Finally, "finally")                                                                                              \
    X(Property, "property")                                                                                            \
    X(Private, "private")                                                                                              \
    X(Public, "public")                                                                                                \
    X(Protected, "protected")                                                                                          \
    X(Static, "static")                                                                                                \
    X(Return, "return")                                                                                                \
    X(Break, "break")                                                                                                  \
    X(Export, "export")                                                                                                \
    X(Import, "import")                                                                                                \
    X(Switch, "switch")                                                                                                \
    X(In, "in")                                                                                                        \
    X(InContextOf, "incontextof")                                                                                      \
    X(For, "for")                                                                                                      \
    X(While, "while")                                                                                                  \
    X(Do, "do")                                                                                                        \
    X(If, "if")                                                                                                        \
    X(Else, "else")                                                                                                    \
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
    X(New, "new")                                                                                                      \
    X(Delete, "delete")                                                                                                \
    X(Typeof, "typeof")                                                                                                \
    X(Plus, "+")                                                                                                       \
    X(Minus, "-")                                                                                                      \
    X(Sharp, "#")                                                                                                      \
    X(Dollar, "$")                                                                                                     \
    X(Isvalid, "isvalid")                                                                                              \
    X(Invalidate, "invalidate")                                                                                        \
    X(Instanceof, "instanceof")                                                                                        \
    X(LParenthesis, "(")                                                                                               \
    X(Dot, ".")                                                                                                        \
    X(LBracket, "[")                                                                                                   \
    X(This, "this")                                                                                                    \
    X(Super, "super")                                                                                                  \
    X(Global, "global")                                                                                                \
    X(RBracket, "]")                                                                                                   \
    X(Class, "class")                                                                                                  \
    X(RParenthesis, ")")                                                                                               \
    X(Colon, ":")                                                                                                      \
    X(SemiColon, ";")                                                                                                  \
    X(LeftCurlyBrace, "{")                                                                                             \
    X(RightCurlyBrace, "}")                                                                                            \
    X(LineComment, "line_comment")                                                                                     \
    X(BlockComment, "block_comment")                                                                                   \
    X(Identifier, "identifier")                                                                                        \
    X(ConstVal, "const_val")                                                                                           \
    X(Int, "int")                                                                                                      \
    X(Real, "real")                                                                                                    \
    X(String, "string")

    enum class TokenType {
#define TOKEN_TYPE_ENUM(NAME, SYMBOL) NAME,
        TOKEN_SYMBOL_PAIR_ENUM(TOKEN_TYPE_ENUM)
#undef TOKEN_TYPE_ENUM
    };

    static constexpr auto S_TypeToName = frozen::make_unordered_map<TokenType, frozen::string>({
#define TYPE_TO_NAME_PAIR(NAME, SYMBOL) { TokenType::NAME, SYMBOL },
        TOKEN_SYMBOL_PAIR_ENUM(TYPE_TO_NAME_PAIR)
#undef TYPE_TO_NAME_PAIR
    });

    static constexpr const char *tokenTypeToStr(const TokenType type) {
        const auto it = S_TypeToName.find(type);
        if(it != S_TypeToName.end()) {
            return it->second.data();
        }
        return "unknown";
    }

    struct Token {
        Common::SourceLocation location{};

        explicit Token() = default;

        constexpr explicit Token(const TokenType type) : _type(type) {}

        explicit Token(const TokenType type, const Value &value) : _type(type), _value(value) {}

        Token(const Token &token) noexcept {
            _type = token._type;
            _value = token._value;
            location = token.location;
        }

        bool operator==(const TokenType tokenType) const { return _type == tokenType; }

        bool operator==(const Token &token) const { return _type == token.type() && _value == token.value(); }

        [[nodiscard]] constexpr TokenType type() const noexcept { return _type; }

        [[nodiscard]] Value value() const noexcept { return _value; }

        [[nodiscard]] constexpr const char *name() const noexcept { return tokenTypeToStr(_type); }

    private:
        TokenType _type = TokenType::Void;
        Value _value{};
    };

    enum class StringParseState {
        None,
        Delimiter, //' "
        Ampersand, //&
        Dollar //${}
    };

    class AssignToken {
    public:
        static constexpr bool support(const TokenType tokenType) noexcept {
            return _assignToNonAssign.find(tokenType) != _assignToNonAssign.end();
        }

        static constexpr TokenType strip(const TokenType tokenType) noexcept {
            const auto it = _assignToNonAssign.find(tokenType);
            if(it != _assignToNonAssign.end()) {
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
            { TokenType::LogicalAnd, TokenType::LogicalAnd }, // &&=
            { TokenType::RBitShiftEqual, TokenType::RBitShift }, // >>>=
            { TokenType::LArithShiftEqual, TokenType::LArithShift }, // <<=
            { TokenType::RArithShiftEqual, TokenType::RArithShift } // >>=
        });
    };
} // namespace Ciallang::Syntax
