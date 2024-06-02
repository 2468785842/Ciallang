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

#include "IEEETypes.hpp"
#include "../common/SourceLocation.hpp"
#include "../types/TjsValue.hpp"

namespace Ciallang::Syntax {
    using namespace Common;

    enum class TokenType {
        Invalid,
        EndOfFile,

        Void,
        Synchronized,
        With,

        Var,
        Const,
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

        LineComment,
        BlockComment,
        Identifier,
        ConstVal,
        Int,
        Real,
        String
    };

    static constexpr char
            CommaLiteral[] = ",",
            AssignmentLiteral[] = "=",
            AmpersandEqualLiteral[] = "&=",
            VertLineEqualLiteral[] = "|=",
            ChevronEqualLiteral[] = "^=",
            MinusEqualLiteral[] = "-=",
            PlusEqualLiteral[] = "+=",
            PercentEqualLiteral[] = "%=",
            SlashEqualLiteral[] = "/=",
            BackslashEqualLiteral[] = "\\=",
            AsteriskEqualLiteral[] = "*=",
            LogicalOrEqualLiteral[] = "||=",
            LogicalAndEqualLiteral[] = "&&=",
            RBitShiftEqualLiteral[] = ">>>=",
            LArithShiftEqualLiteral[] = "<<=",
            RArithShiftEqualLiteral[] = ">>=",
            QuestionLiteral[] = "?",
            LogicalOrLiteral[] = "||",
            LogicalAndLiteral[] = "&&",
            VertLineLiteral[] = "|",
            ChevronLiteral[] = "^",
            AmpersandLiteral[] = "&",
            NotEqualLiteral[] = "!=",
            EqualLiteral[] = "==",
            DiscNotEqualLiteral[] = "!==",
            DiscEqualLiteral[] = "===",
            SwapLiteral[] = "<->",
            LtLiteral[] = "<",
            GtLiteral[] = ">",
            LtOrEqualLiteral[] = "<=",
            GtOrEqualLiteral[] = ">=",
            RArithShiftLiteral[] = ">>",
            LArithShiftLiteral[] = "<<",
            RBitShiftLiteral[] = ">>>",
            PercentLiteral[] = "%",
            SlashLiteral[] = "/",
            BackslashLiteral[] = "\\",
            AsteriskLiteral[] = "*",
            ExclamationLiteral[] = "!",
            TildeLiteral[] = "~",
            DecrementLiteral[] = "--",
            IncrementLiteral[] = "++",
            PlusLiteral[] = "+",
            MinusLiteral[] = "-",
            SharpLiteral[] = "#",
            DollarLiteral[] = "$",
            LParenthesisLiteral[] = "(",
            DotLiteral[] = ".",
            LBracketLiteral[] = "[",
            RBracketLiteral[] = "]",
            RParenthesisLiteral[] = ")",
            ColonLiteral[] = ":",
            SemiColonLiteral[] = ";",
            LeftCurlyBraceLiteral[] = "{",
            RightCurlyBraceLiteral[] = "}",
            NewLiteral[] = "new",
            DeleteLiteral[] = "delete",
            TypeofLiteral[] = "typeof",
            IsvalidLiteral[] = "isvalid",
            InvalidateLiteral[] = "invalidate",
            InstanceofLiteral[] = "instanceof",
            ThisLiteral[] = "this",
            SuperLiteral[] = "super",
            GlobalLiteral[] = "global",
            ClassLiteral[] = "class",
            VoidLiteral[] = "void",
            SynchronizedLiteral[] = "synchronized",
            WithLiteral[] = "with",
            VarLiteral[] = "var",
            ConstLiteral[] = "const",
            EnumLiteral[] = "enum",
            GotoLiteral[] = "goto",
            ThrowLiteral[] = "throw",
            TryLiteral[] = "try",
            SetterLiteral[] = "setter",
            GetterLiteral[] = "getter",
            CatchLiteral[] = "catch",
            OmitLiteral[] = "...",
            ContinueLiteral[] = "continue",
            FunctionLiteral[] = "function",
            DebuggerLiteral[] = "debugger",
            DefaultLiteral[] = "default",
            CaseLiteral[] = "case",
            ExtendsLiteral[] = "extends",
            FinallyLiteral[] = "finally",
            PropertyLiteral[] = "property",
            PrivateLiteral[] = "private",
            PublicLiteral[] = "public",
            ProtectedLiteral[] = "protected",
            StaticLiteral[] = "static",
            ReturnLiteral[] = "return",
            BreakLiteral[] = "break",
            ExportLiteral[] = "export",
            ImportLiteral[] = "import",
            SwitchLiteral[] = "switch",
            InLiteral[] = "in",
            IncontextofLiteral[] = "incontextof",
            ForLiteral[] = "for",
            WhileLiteral[] = "while",
            DoLiteral[] = "do",
            IfLiteral[] = "if",
            ElseLiteral[] = "else",
            InvalidLiteral[] = "invalid",
            LineCommentLiteral[] = "line_comment",
            BlockCommentLiteral[] = "block_comment",
            IdentifierLiteral[] = "identifier",
            ConstValLiteral[] = "const_val",
            IntLiteral[] = "int",
            RealLiteral[] = "real",
            StringLiteral[] = "string",
            EndOfFileLiteral[] = "end_of_file",
            Unknown[] = "unknown";


    static constexpr auto S_TypeToName =
            frozen::make_unordered_map<TokenType, const char*>({
                    { TokenType::Comma, CommaLiteral },
                    { TokenType::Assignment, AssignmentLiteral },
                    { TokenType::AmpersandEqual, AmpersandEqualLiteral },
                    { TokenType::VertLineEqual, VertLineEqualLiteral },
                    { TokenType::ChevronEqual, ChevronEqualLiteral },
                    { TokenType::MinusEqual, MinusEqualLiteral },
                    { TokenType::PlusEqual, PlusEqualLiteral },
                    { TokenType::PercentEqual, PercentEqualLiteral },
                    { TokenType::SlashEqual, SlashEqualLiteral },
                    { TokenType::BackslashEqual, BackslashEqualLiteral },
                    { TokenType::AsteriskEqual, AsteriskEqualLiteral },
                    { TokenType::LogicalOrEqual, LogicalOrEqualLiteral },
                    { TokenType::LogicalAndEqual, LogicalAndEqualLiteral },
                    { TokenType::RBitShiftEqual, RBitShiftEqualLiteral },
                    { TokenType::LArithShiftEqual, LArithShiftEqualLiteral },
                    { TokenType::RArithShiftEqual, RArithShiftEqualLiteral },
                    { TokenType::Question, QuestionLiteral },
                    { TokenType::LogicalOr, LogicalOrLiteral },
                    { TokenType::LogicalAnd, LogicalAndLiteral },
                    { TokenType::VertLine, VertLineLiteral },
                    { TokenType::Chevron, ChevronLiteral },
                    { TokenType::Ampersand, AmpersandLiteral },
                    { TokenType::NotEqual, NotEqualLiteral },
                    { TokenType::Equal, EqualLiteral },
                    { TokenType::DiscNotEqual, DiscNotEqualLiteral },
                    { TokenType::DiscEqual, DiscEqualLiteral },
                    { TokenType::Swap, SwapLiteral },
                    { TokenType::Lt, LtLiteral },
                    { TokenType::Gt, GtLiteral },
                    { TokenType::LtOrEqual, LtOrEqualLiteral },
                    { TokenType::GtOrEqual, GtOrEqualLiteral },
                    { TokenType::RArithShift, RArithShiftLiteral },
                    { TokenType::LArithShift, LArithShiftLiteral },
                    { TokenType::RBitShift, RBitShiftLiteral },
                    { TokenType::Percent, PercentLiteral },
                    { TokenType::Slash, SlashLiteral },
                    { TokenType::Backslash, BackslashLiteral },
                    { TokenType::Asterisk, AsteriskLiteral },
                    { TokenType::Exclamation, ExclamationLiteral },
                    { TokenType::Tilde, TildeLiteral },
                    { TokenType::Decrement, DecrementLiteral },
                    { TokenType::Increment, IncrementLiteral },
                    { TokenType::Plus, PlusLiteral },
                    { TokenType::Minus, MinusLiteral },
                    { TokenType::Sharp, SharpLiteral },
                    { TokenType::Dollar, DollarLiteral },
                    { TokenType::LParenthesis, LParenthesisLiteral },
                    { TokenType::Dot, DotLiteral },
                    { TokenType::LBracket, LBracketLiteral },
                    { TokenType::RBracket, RBracketLiteral },
                    { TokenType::RParenthesis, RParenthesisLiteral },
                    { TokenType::Colon, ColonLiteral },
                    { TokenType::SemiColon, SemiColonLiteral },
                    { TokenType::LeftCurlyBrace, LeftCurlyBraceLiteral },
                    { TokenType::RightCurlyBrace, RightCurlyBraceLiteral },
                    { TokenType::New, NewLiteral },
                    { TokenType::Delete, DeleteLiteral },
                    { TokenType::Typeof, TypeofLiteral },
                    { TokenType::Isvalid, IsvalidLiteral },
                    { TokenType::Invalidate, InvalidateLiteral },
                    { TokenType::Instanceof, InstanceofLiteral },
                    { TokenType::This, ThisLiteral },
                    { TokenType::Super, SuperLiteral },
                    { TokenType::Global, GlobalLiteral },
                    { TokenType::Class, ClassLiteral },
                    { TokenType::Void, VoidLiteral },
                    { TokenType::Synchronized, SynchronizedLiteral },
                    { TokenType::With, WithLiteral },
                    { TokenType::Var, VarLiteral },
                    { TokenType::Const, ConstLiteral },
                    { TokenType::Enum, EnumLiteral },
                    { TokenType::Goto, GotoLiteral },
                    { TokenType::Throw, ThrowLiteral },
                    { TokenType::Try, TryLiteral },
                    { TokenType::Setter, SetterLiteral },
                    { TokenType::Getter, GetterLiteral },
                    { TokenType::Catch, CatchLiteral },
                    { TokenType::Omit, OmitLiteral },
                    { TokenType::Continue, ContinueLiteral },
                    { TokenType::Function, FunctionLiteral },
                    { TokenType::Debugger, DebuggerLiteral },
                    { TokenType::Default, DefaultLiteral },
                    { TokenType::Case, CaseLiteral },
                    { TokenType::Extends, ExtendsLiteral },
                    { TokenType::Finally, FinallyLiteral },
                    { TokenType::Property, PropertyLiteral },
                    { TokenType::Private, PrivateLiteral },
                    { TokenType::Public, PublicLiteral },
                    { TokenType::Protected, ProtectedLiteral },
                    { TokenType::Static, StaticLiteral },
                    { TokenType::Return, ReturnLiteral },
                    { TokenType::Break, BreakLiteral },
                    { TokenType::Export, ExportLiteral },
                    { TokenType::Import, ImportLiteral },
                    { TokenType::Switch, SwitchLiteral },
                    { TokenType::In, InLiteral },
                    { TokenType::Incontextof, IncontextofLiteral },
                    { TokenType::For, ForLiteral },
                    { TokenType::While, WhileLiteral },
                    { TokenType::Do, DoLiteral },
                    { TokenType::If, IfLiteral },
                    { TokenType::Else, ElseLiteral },
                    { TokenType::Invalid, InvalidLiteral },
                    { TokenType::LineComment, LineCommentLiteral },
                    { TokenType::BlockComment, BlockCommentLiteral },
                    { TokenType::Identifier, IdentifierLiteral },
                    { TokenType::ConstVal, ConstValLiteral },
                    { TokenType::Int, IntLiteral },
                    { TokenType::Real, RealLiteral },
                    { TokenType::String, StringLiteral },
                    { TokenType::EndOfFile, EndOfFileLiteral }
            });

    struct Token {
        SourceLocation location{};

        explicit Token() = default;

        constexpr explicit Token(const TokenType type) :
            _type(type) {
        }

        explicit Token(const TokenType type, TjsValue&& value) :
            _type(type), _value(new TjsValue{ std::move(value) }) {
        }

        Token(Token&& token) noexcept {
            _type = token._type;
            _value = token._value;
            location = token.location;

            token._value = nullptr;
        }

        Token(const Token& token) noexcept {
            _type = token._type;

            delete _value;
            if(token._value)
                _value = new TjsValue{ *token._value };

            location = token.location;
        }

        Token& operator=(Token&& token) noexcept {
            _type = token._type;
            _value = token._value;
            location = token.location;

            token._value = nullptr;
            return *this;
        }

        Token& operator=(const Token& token) noexcept {
            if(this == &token)
                return *this;

            _type = token._type;

            delete _value;
            if(token._value)
                _value = new TjsValue{ *token._value };

            location = token.location;

            return *this;
        }

        [[nodiscard]] constexpr TokenType type() const noexcept { return _type; }

        [[nodiscard]] constexpr const TjsValue* value() const noexcept {
            return _value;
        }

        [[nodiscard]] constexpr const char* name() const noexcept {
            const auto it = S_TypeToName.find(_type);
            if(it != S_TypeToName.end()) {
                return it->second;
            }
            return Unknown;
        }

        ~Token() noexcept { delete _value; }

    private:
        TokenType _type{ TokenType::Void };
        TjsValue* _value{ nullptr };
    };

    /**
     *  TJS保留字符
     */
    // use token->type() != TokenType::EndOfFile
    // not token != &S_EndOfFile, because location maybe change
    static const constinit inline Token
            S_Invalid{ TokenType::Invalid },
            S_EndOfFile{ TokenType::EndOfFile },
            S_LineComment{ TokenType::LineComment },
            S_BlockComment{ TokenType::BlockComment },
            S_Var{ TokenType::Var },
            S_Const{ TokenType::Const },

            S_SemiColon{ TokenType::SemiColon },
            S_LeftCurlyBrace{ TokenType::LeftCurlyBrace },
            S_RightCurlyBrace{ TokenType::RightCurlyBrace },

            S_RBitShiftEqual{ TokenType::RBitShiftEqual },
            S_RBitShift{ TokenType::RBitShift },
            S_RArithShiftEqual{ TokenType::RArithShiftEqual },
            S_RArithShift{ TokenType::RArithShift },
            S_GtOrEqual{ TokenType::GtOrEqual },
            S_Gt{ TokenType::Gt },

            S_LArithShiftEqual{ TokenType::LArithShiftEqual },
            S_Swap{ TokenType::Swap },
            S_LtOrEqual{ TokenType::LtOrEqual },
            S_LArithShift{ TokenType::LArithShift },
            S_Lt{ TokenType::Lt },

            S_Assignment{ TokenType::Assignment },
            S_Equal{ TokenType::Equal },
            S_DiscEqual{ TokenType::DiscEqual },

            S_DiscNotEqual{ TokenType::DiscNotEqual },
            S_NotEqual{ TokenType::NotEqual },
            S_Exclamation{ TokenType::Exclamation },

            S_LogicalAndEqual{ TokenType::LogicalAndEqual },
            S_LogicalAnd{ TokenType::LogicalAnd },
            S_AmpersandEqual{ TokenType::AmpersandEqual },
            S_Ampersand{ TokenType::Ampersand },

            S_LogicalOrEqual{ TokenType::LogicalOrEqual },
            S_LogicalOr{ TokenType::LogicalOr },
            S_VertLineEqual{ TokenType::VertLineEqual },
            S_VertLine{ TokenType::VertLine },

            S_Omit{ TokenType::Omit },
            S_Dot{ TokenType::Dot },

            S_Increment{ TokenType::Increment },
            S_PlusEqual{ TokenType::PlusEqual },
            S_Plus{ TokenType::Plus },

            S_Decrement{ TokenType::Decrement },
            S_MinusEqual{ TokenType::MinusEqual },
            S_Minus{ TokenType::Minus },

            S_AsteriskEqual{ TokenType::AsteriskEqual },
            S_Asterisk{ TokenType::Asterisk },

            S_SlashEqual{ TokenType::SlashEqual },
            S_Slash{ TokenType::Slash },
            S_BackslashEqual{ TokenType::BackslashEqual },
            S_Backslash{ TokenType::Backslash },

            S_PercentEqual{ TokenType::PercentEqual },
            S_Percent{ TokenType::Percent },
            S_ChevronEqual{ TokenType::ChevronEqual },
            S_Chevron{ TokenType::Chevron },
            S_LBracket{ TokenType::LBracket },
            S_RBracket{ TokenType::RBracket },
            S_LParenthesis{ TokenType::LParenthesis },
            S_RParenthesis{ TokenType::RParenthesis },
            S_Tilde{ TokenType::Tilde },
            S_Question{ TokenType::Question },
            S_Colon{ TokenType::Colon },
            S_Comma{ TokenType::Comma },
            S_Sharp{ TokenType::Sharp },
            S_Dollar{ TokenType::Dollar },

            S_If{ TokenType::If },
            S_Else{ TokenType::Else },

            S_New{ TokenType::New },
            S_Function{ TokenType::Function },

            S_Int{ TokenType::Int },
            S_Real{ TokenType::Real },
            S_String{ TokenType::String };

    static const inline Token
            S_True{ TokenType::ConstVal, tjsInteger(1) },
            S_False{ TokenType::ConstVal, tjsInteger(0) },

            S_Infinity{ TokenType::ConstVal, tjsReal(static_cast<double>(IEEE_D_N_INF)) },

            S_NaN{ TokenType::ConstVal, tjsReal(static_cast<double>(IEEE_D_N_NaN)) };

    enum class StringParseState {
        None,
        Delimiter, //' "
        Ampersand, //&
        Dollar     //${}
    };

    static constexpr inline auto S_AssignToNonAssign =
            frozen::make_unordered_map<TokenType, const Token&>({
                    { TokenType::PlusEqual, S_Plus },               // +=
                    { TokenType::MinusEqual, S_Minus },             // -=
                    { TokenType::SlashEqual, S_Slash },             // /=
                    { TokenType::PercentEqual, S_Percent },         // %=
                    { TokenType::AsteriskEqual, S_Asterisk },       // *=
                    { TokenType::VertLineEqual, S_VertLine },       // |=
                    { TokenType::AmpersandEqual, S_Ampersand },     // &=
                    { TokenType::ChevronEqual, S_Chevron },         // ^=
                    { TokenType::BackslashEqual, S_Backslash },     // \=
                    { TokenType::LogicalOrEqual, S_LogicalOr },     // ||=
                    { TokenType::LogicalAnd, S_LogicalAnd },        // &&=
                    { TokenType::RBitShiftEqual, S_RBitShift },     // >>>=
                    { TokenType::LArithShiftEqual, S_LArithShift }, // <<=
                    { TokenType::RArithShiftEqual, S_RArithShift }  // >>=
            });
    using AssignToNonAssignMap = decltype (S_AssignToNonAssign);

    static bool contains(const AssignToNonAssignMap& map,
                         const TokenType tokenType) noexcept {
        return map.find(tokenType) != map.end();
    }

    static Token stripAssign(Token&& token) noexcept {
        const auto it = S_AssignToNonAssign.find(token.type());
        if(it != S_AssignToNonAssign.end()) {
            return it->second;
        }

        return std::move(token);
    }
} // namespace Ciallang::Syntax
