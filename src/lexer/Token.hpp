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

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

#include "SourceLocation.hpp"
#include "TjsValue.hpp"
#include "IEEETypes.hpp"

namespace Ciallang::Syntax {
    using namespace std;
    using namespace Common;

    enum class TokenType {
        Invalid,
        EndOfFile,

        Void,
        Synchronized,
        With,

        Var, Const,
        Enum,
        Goto,
        Throw,
        Try,
        Setter,
        Getter,
        Catch,
        Omit,

        Continue,
        Function,
        Debugger,
        Default,
        Case,
        Extends,
        Finally,
        Property,
        Private,
        Public,
        Protected,
        Static,
        Return,
        Break,
        Export,
        Import,
        Switch,
        In,
        Incontextof,
        For,
        While,
        Do,
        If,
        Else,

        Comma,

        Assignment,
        AmpersandEqual,
        VertLineEqual,
        ChevronEqual,
        MinusEqual,
        PlusEqual,
        PercentEqual,
        SlashEqual,
        BackslashEqual,
        AsteriskEqual,
        LogicalOrEqual,
        LogicalAndEqual,
        RBitShiftEqual,
        LArithShiftEqual,
        RArithShiftEqual,

        Question,
        LogicalOr,
        LogicalAnd,
        VertLine,
        Chevron,
        Ampersand,
        NotEqual,
        Equal,
        DiscNotEqual,
        DiscEqual,
        Swap,
        Lt,
        Gt,
        LtOrEqual,
        GtOrEqual,
        RArithShift,
        LArithShift,
        RBitShift,
        Percent,
        Slash,
        Backslash,
        Asterisk,
        Exclamation,
        Tilde,
        Decrement,
        Increment,
        New,
        Delete,
        Typeof,
        Plus,
        Minus,
        Sharp,
        Dollar,
        Isvalid,
        Invalidate,
        Instanceof,
        LParenthesis,
        Dot,
        LBracket,
        This,
        Super,
        Global,
        RBracket,
        Class,
        RParenthesis,
        Colon,
        SemiColon,
        LeftCurlyBrace,
        RightCurlyBrace,

        ////////////////////////////////////////////////////////////////
        // unary
        ////////////////////////////////////////////////////////////////

        Eval,
        Postdecrement,
        Postincrement,
        Ignoreprop,
        Propaccess,
        Arg,
        Expandarg,
        Inlinearray,
        Arrayarg,
        Inlinedic,
        Dicelm,
        Withdot,
        ThisProxy,
        WithdotProxy,

        ////////////////////////////////////////////////////////////////

        LineComment, BlockComment,
        Identifier,
        ConstVal,
        Int,
        Real,
        String
    };

    static const inline unordered_map<TokenType, const char *> S_TypeToName = {
            {TokenType::Comma,            ","},
            {TokenType::Assignment,       "="},
            {TokenType::AmpersandEqual,   "&="},
            {TokenType::VertLineEqual,    "|="},
            {TokenType::ChevronEqual,     "^="},
            {TokenType::MinusEqual,       "-="},
            {TokenType::PlusEqual,        "+="},
            {TokenType::PercentEqual,     "%="},
            {TokenType::SlashEqual,       "/="},
            {TokenType::BackslashEqual,   "\\="},
            {TokenType::AsteriskEqual,    "*="},
            {TokenType::LogicalOrEqual,   "||="},
            {TokenType::LogicalAndEqual,  "&&="},
            {TokenType::RBitShiftEqual,   ">>>="},
            {TokenType::LArithShiftEqual, "<<="},
            {TokenType::RArithShiftEqual, ">>="},
            {TokenType::Question,         "?"},
            {TokenType::LogicalOr,        "||"},
            {TokenType::LogicalAnd,       "&&"},
            {TokenType::VertLine,         "|"},
            {TokenType::Chevron,          "^"},
            {TokenType::Ampersand,        "&"},
            {TokenType::NotEqual,         "!="},
            {TokenType::Equal,            "=="},
            {TokenType::DiscNotEqual,     "!=="},
            {TokenType::DiscEqual,        "==="},
            {TokenType::Swap,             "<->"},
            {TokenType::Lt,               "<"},
            {TokenType::Gt,               ">"},
            {TokenType::LtOrEqual,        "<="},
            {TokenType::GtOrEqual,        ">="},
            {TokenType::RArithShift,      ">>"},
            {TokenType::LArithShift,      "<<"},
            {TokenType::RBitShift,        ">>>"},
            {TokenType::Percent,          "%"},
            {TokenType::Slash,            "/"},
            {TokenType::Backslash,        "\\"},
            {TokenType::Asterisk,         "*"},
            {TokenType::Exclamation,      "!"},
            {TokenType::Tilde,            "~"},
            {TokenType::Decrement,        "--"},
            {TokenType::Increment,        "++"},

            {TokenType::Plus,             "+"},
            {TokenType::Minus,            "-"},
            {TokenType::Sharp,            "#"},
            {TokenType::Dollar,           "$"},
            {TokenType::LParenthesis,     "("},
            {TokenType::Dot,              "."},
            {TokenType::LBracket,         "["},
            {TokenType::RBracket,         "]"},
            {TokenType::RParenthesis,     ")"},
            {TokenType::Colon,            ":"},
            {TokenType::SemiColon,        ";"},
            {TokenType::LeftCurlyBrace,   "{"},
            {TokenType::RightCurlyBrace,  "}"},

            {TokenType::New,              "new"},
            {TokenType::Delete,           "delete"},
            {TokenType::Typeof,           "typeof"},
            {TokenType::Isvalid,          "isvalid"},
            {TokenType::Invalidate,       "invalidate"},
            {TokenType::Instanceof,       "instanceof"},
            {TokenType::This,             "this"},
            {TokenType::Super,            "super"},
            {TokenType::Global,           "global"},
            {TokenType::Class,            "class"},

            {TokenType::Void,             "void"},

            {TokenType::Synchronized,     "synchronized"},
            {TokenType::With,             "with"},


            {TokenType::Var,              "var"},
            {TokenType::Const,            "const"},
            {TokenType::Enum,             "enum"},
            {TokenType::Goto,             "goto"},
            {TokenType::Throw,            "throw"},
            {TokenType::Try,              "try"},
            {TokenType::Setter,           "setter"},
            {TokenType::Getter,           "getter"},
            {TokenType::Catch,            "catch"},
            {TokenType::Omit,             "..."},

            {TokenType::Continue,         "continue"},
            {TokenType::Function,         "function"},
            {TokenType::Debugger,         "debugger"},
            {TokenType::Default,          "default"},
            {TokenType::Case,             "case"},
            {TokenType::Extends,          "extends"},
            {TokenType::Finally,          "finally"},
            {TokenType::Property,         "property"},
            {TokenType::Private,          "private"},
            {TokenType::Public,           "public"},
            {TokenType::Protected,        "protected"},
            {TokenType::Static,           "static"},
            {TokenType::Return,           "return"},
            {TokenType::Break,            "break"},
            {TokenType::Export,           "export"},
            {TokenType::Import,           "import"},
            {TokenType::Switch,           "switch"},
            {TokenType::In,               "in"},
            {TokenType::Incontextof,      "incontextof"},
            {TokenType::For,              "for"},
            {TokenType::While,            "while"},
            {TokenType::Do,               "do"},
            {TokenType::If,               "if"},
            {TokenType::Else,             "else"},

            {TokenType::Invalid,          "invalid"},

            {TokenType::LineComment,      "line_comment"},
            {TokenType::BlockComment,     "block_comment"},

            {TokenType::Identifier,       "identifier"},
            {TokenType::ConstVal,         "const_val"},

            {TokenType::Int,              "int"},
            {TokenType::Real,             "real"},
            {TokenType::String,           "string"},

            {TokenType::Invalid,          "invalid"},
            {TokenType::EndOfFile,        "end_of_file"},
    };

    struct Token {

        SourceLocation location{};

        explicit Token(TokenType type) : _type(type) {}

        explicit Token(TokenType type, TjsValue &&value) :
                _type(type),
                _value(new TjsValue{std::move(value)}) {}

        Token(Token &&token) noexcept {
            _type = token._type;
            _value = token._value;
            location = token.location;

            token._value = nullptr;
        }

        Token(const Token &token) noexcept {
            _type = token._type;

            delete _value;
            if (token._value)
                _value = new TjsValue{*token._value};

            location = token.location;
        }

        Token &operator=(const Token &token) noexcept {
            if (this == &token) return *this;

            _type = token._type;

            delete _value;
            if (token._value)
                _value = new TjsValue{*token._value};

            location = token.location;

            return *this;
        }

        [[nodiscard]] constexpr TokenType type() const noexcept {
            return _type;
        }

        [[nodiscard]] constexpr Ciallang::TjsValue *value() const noexcept {
            return _value;
        }

        [[nodiscard]] constexpr const char *name() const noexcept {
            if (S_TypeToName.contains(_type)) {
                return S_TypeToName.at(_type);
            }
            return "unknown";
        }

        ~Token() noexcept { delete _value; }

    private :
        TokenType _type{TokenType::Void};
        Ciallang::TjsValue *_value{nullptr};

    };

    /**
     *  TJS保留字符
     */
    // use token->type() != TokenType::EndOfFile
    // not token != &S_EndOfFile, because location maybe change
    static const inline Token
            S_Invalid{TokenType::Invalid},
            S_EndOfFile{TokenType::EndOfFile},
            S_LineComment{TokenType::LineComment},
            S_BlockComment{TokenType::BlockComment},
            S_Var{TokenType::Var},
            S_Const{TokenType::Const},

            S_SemiColon{TokenType::SemiColon},
            S_LeftCurlyBrace{TokenType::LeftCurlyBrace},
            S_RightCurlyBrace{TokenType::RightCurlyBrace},

            S_RBitShiftEqual{TokenType::RBitShiftEqual},
            S_RBitShift{TokenType::RBitShift},
            S_RArithShiftEqual{TokenType::RArithShiftEqual},
            S_RArithShift{TokenType::RArithShift},
            S_GtOrEqual{TokenType::GtOrEqual},
            S_Gt{TokenType::Gt},

            S_LArithShiftEqual{TokenType::LArithShiftEqual},
            S_Swap{TokenType::Swap},
            S_LtOrEqual{TokenType::LtOrEqual},
            S_LArithShift{TokenType::LArithShift},
            S_Lt{TokenType::Lt},

            S_Assignment{TokenType::Assignment},
            S_Equal{TokenType::Equal},
            S_DiscEqual{TokenType::DiscEqual},

            S_DiscNotEqual{TokenType::DiscNotEqual},
            S_NotEqual{TokenType::NotEqual},
            S_Exclamation{TokenType::Exclamation},

            S_LogicalAndEqual{TokenType::LogicalAndEqual},
            S_LogicalAnd{TokenType::LogicalAnd},
            S_AmpersandEqual{TokenType::AmpersandEqual},
            S_Ampersand{TokenType::Ampersand},

            S_LogicalOrEqual{TokenType::LogicalOrEqual},
            S_LogicalOr{TokenType::LogicalOr},
            S_VertLineEqual{TokenType::VertLineEqual},
            S_VertLine{TokenType::VertLine},

            S_Omit{TokenType::Omit},
            S_Dot{TokenType::Dot},

            S_Increment{TokenType::Increment},
            S_PlusEqual{TokenType::PlusEqual},
            S_Plus{TokenType::Plus},

            S_Decrement{TokenType::Decrement},
            S_MinusEqual{TokenType::MinusEqual},
            S_Minus{TokenType::Minus},

            S_AsteriskEqual{TokenType::AsteriskEqual},
            S_Asterisk{TokenType::Asterisk},

            S_SlashEqual{TokenType::SlashEqual},
            S_Slash{TokenType::Slash},
            S_BackslashEqual{TokenType::BackslashEqual},
            S_Backslash{TokenType::Backslash},

            S_PercentEqual{TokenType::PercentEqual},
            S_Percent{TokenType::Percent},
            S_ChevronEqual{TokenType::ChevronEqual},
            S_Chevron{TokenType::Chevron},
            S_LBracket{TokenType::LBracket},
            S_RBracket{TokenType::RBracket},
            S_LParenthesis{TokenType::LParenthesis},
            S_RParenthesis{TokenType::RParenthesis},
            S_Tilde{TokenType::Tilde},
            S_Question{TokenType::Question},
            S_Colon{TokenType::Colon},
            S_Comma{TokenType::Comma},
            S_Sharp{TokenType::Sharp},
            S_Dollar{TokenType::Dollar},

            S_If{TokenType::If},
            S_Else{TokenType::Else},

            S_New{TokenType::New},
            S_Function{TokenType::Function},


            S_Int{TokenType::Int},
            S_Real{TokenType::Real},
            S_String{TokenType::String},


            S_True{TokenType::ConstVal, tjsInteger(1)},
            S_False{TokenType::ConstVal, tjsInteger(0)},

            S_Infinity{
            TokenType::ConstVal,
            tjsReal(static_cast<double>(IEEE_D_N_INF))},

            S_NaN{
            TokenType::ConstVal,
            tjsReal(static_cast<double>(IEEE_D_N_NaN))};

    enum class StringParseState {
        None,
        Delimiter, //' "
        Ampersand, //&
        Dollar //${}
    };

    static const inline std::unordered_map<TokenType, const Token &> S_AssignToNonAssign{
            {TokenType::PlusEqual,        S_Plus},           // +=
            {TokenType::MinusEqual,       S_Minus},          // -=
            {TokenType::SlashEqual,       S_Slash},          // /=
            {TokenType::PercentEqual,     S_Percent},        // %=
            {TokenType::AsteriskEqual,    S_Asterisk},       // *=
            {TokenType::VertLineEqual,    S_VertLine},       // |=
            {TokenType::AmpersandEqual,   S_Ampersand},      // &=
            {TokenType::ChevronEqual,     S_Chevron},        // ^=
            {TokenType::BackslashEqual,   S_Backslash},      // \=
            {TokenType::LogicalOrEqual,   S_LogicalOr},      // ||=
            {TokenType::LogicalAnd,       S_LogicalAnd},     // &&=
            {TokenType::RBitShiftEqual,   S_RBitShift},      // >>>=
            {TokenType::LArithShiftEqual, S_LArithShift},    // <<=
            {TokenType::RArithShiftEqual, S_RArithShift}     // >>=
    };

    static Token stripAssign(const Token &token) {
        auto it = S_AssignToNonAssign.find(token.type());
        if (it != S_AssignToNonAssign.end()) {
            return it->second;
        }

        return token;
    }
}
