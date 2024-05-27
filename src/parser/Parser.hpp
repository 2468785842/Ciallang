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

#include "Lexer.hpp"
#include "Parser.hpp"
#include "SourceFile.hpp"
#include "Ast.hpp"

namespace Ciallang::Syntax {
    using namespace Common;

    class Parser;

    enum class Precedence : uint8_t {
        lowest = 0,
        comma, // ,
        assignment, // 一系列赋值语句
        conditional_ternary, // cond ? expr : expr
        // key_value,
        logical_or, // ||
        logical_and, // &&
        bitwise_or, // |
        bitwise_xor, // ^
        bitwise_and, // &
        equality, // == !=
        relational, // > >= < <=
        bitwise_shift_or_roll, // >> << >>>
        sum_sub, // + -
        product, // * / \ %
        prefix, // - ~ ++ -- new
        postfix, // [] () .
        cast, //
        type,
        variable,
        call
    };

    class StmtParser {
    public:
        virtual ~StmtParser() = default;

        virtual StmtNode *parse(
                Result &r,
                Parser *parser,
                std::unique_ptr<Token> &token) = 0;
    };

    class InfixParser {
    public:
        virtual ~InfixParser() = default;

        virtual ExprNode *parse(
                Result &r,
                Parser *parser,
                ExprNode *lhs,
                std::unique_ptr<Token> &token) = 0;

        // 优先级
        [[nodiscard]] virtual Precedence precedence() const = 0;
    };

    class PrefixParser {
    public:
        virtual ~PrefixParser() = default;

        virtual ExprNode *parse(
                Result &r,
                Parser *parser,
                std::unique_ptr<Token> &token) = 0;
    };

    class Parser {

    public:
        Parser(SourceFile &sourceFile, AstBuilder &builder) :
                _lexer(Lexer{sourceFile}), _astBuilder(builder),
                _sourceFile(sourceFile) {
        }

        void error(
                Result &r,
                const string &message,
                const SourceLocation &location) const {
            _sourceFile.error(r, message, location);
        }

        bool consume();

        bool consume(std::unique_ptr<Token> &token);

        bool current(Token *&token);

        bool lookAhead(size_t count);

        bool peek(TokenType type);

        void synchronize();

        AstNode *parse(Result &r);

        StmtNode *parseScope(Result &r, const Token *token);

        ExprNode *parseExpression(Result &, Precedence = Precedence::lowest);

        StmtNode *parseStatement(Result &r);

        static void writeAstGraph(const filesystem::path &path, AstNode *programNode);

        bool expect(Result &r, const Token *token);

        [[nodiscard]] AstBuilder *astBuilder() const { return &_astBuilder; }

    private:
        Lexer _lexer;
        AstBuilder &_astBuilder;
        SourceFile &_sourceFile;

        Precedence currentInfixPrecedence();

        static StmtParser *stmtParserFor(TokenType type);

        static InfixParser *infixParserFor(TokenType type);

        static PrefixParser *prefixParserFor(TokenType type);

        [[nodiscard]] constexpr const std::vector<Token *> &tokens() const {
            return _lexer.tokens();
        }

    };

    /**
     * +----------------------------------------------------------------------------+
     * |                                stmtParser                                  |
     * +----------------------------------------------------------------------------+
     */
    struct BlockStmtParser final : public StmtParser {
        BlockStmtParser() = default;

        StmtNode *parse(Result &r, Parser *parser, std::unique_ptr<Token> &token) override;
    };

    struct IfStmtParser final : public StmtParser {
        IfStmtParser() = default;

        StmtNode *parse(Result &r, Parser *parser, std::unique_ptr<Token> &token) override;
    };

    static inline BlockStmtParser S_BlockStmtParser{};

    static inline IfStmtParser S_IfStmtParser{};

    static inline std::unordered_map<TokenType, StmtParser *> S_StmtParsers = {
            {TokenType::LeftCurlyBrace, &S_BlockStmtParser},
            {TokenType::If,             &S_IfStmtParser},
    };

    /**
     * +----------------------------------------------------------------------------+
     * |                                prefixParser                                |
     * +----------------------------------------------------------------------------+
     */

    struct ConstValPrefixParser final : public PrefixParser {
        ConstValPrefixParser() = default;

        ExprNode *parse(Result &r, Parser *parser, std::unique_ptr<Token> &token) override;
    };

    struct UnaryOperatorPrefixParser final : public PrefixParser {
        explicit UnaryOperatorPrefixParser(Precedence precedence) noexcept;

        ExprNode *parse(Result &r, Parser *parser, std::unique_ptr<Token> &token) override;

    private:
        Precedence _precedence;
    };

    struct SymbolPrefixParser final : public PrefixParser {
        SymbolPrefixParser() = default;

        ExprNode *parse(Result &r, Parser *parser, std::unique_ptr<Token> &token) override;
    };

//    struct FunctionPrefixParser final : public PrefixParser {
//        FunctionPrefixParser() = default;
//
//        ExprNode *parse(Result &r, Parser *parser, std::unique_ptr<Token> &token) override;
//    };

    static inline ConstValPrefixParser S_ConstValPrefixParser{};
    static inline UnaryOperatorPrefixParser S_NegatePrefixParser{Precedence::sum_sub};
    static inline SymbolPrefixParser S_SymbolPrefixParser;
    static inline UnaryOperatorPrefixParser S_PrefixParser{Precedence::prefix};
//    static inline FunctionPrefixParser S_FunctionPrefixParser{};

    static inline std::unordered_map<TokenType, PrefixParser *> S_PrefixParsers = {
            {TokenType::Const,          &S_PrefixParser},
            {TokenType::Var,            &S_PrefixParser},
            {TokenType::ConstVal,       &S_ConstValPrefixParser},
            {TokenType::Minus,          &S_NegatePrefixParser}, // "-"
            {TokenType::Identifier,     &S_SymbolPrefixParser},
            {TokenType::Exclamation,    &S_PrefixParser}, // "!"
            {TokenType::Tilde,          &S_PrefixParser}, // "~"
            {TokenType::Decrement,      &S_PrefixParser}, // "--"
            {TokenType::Increment,      &S_PrefixParser}, // "++"
            {TokenType::New,            &S_PrefixParser}, // "new" 函数调用, 或创建新对象
            {TokenType::Invalidate,     &S_PrefixParser}, // "invalidate"
            {TokenType::Isvalid,        &S_PrefixParser}, // "isvalid" todo: 注意还有中缀表示
            // incontextof_expr "isvalid"
            {TokenType::Delete,         &S_PrefixParser}, //"delete"
            {TokenType::Typeof,         &S_PrefixParser}, //"typeof
            {TokenType::Sharp,          &S_PrefixParser}, //"#" 获取字符串第一个字符,转为int
            {TokenType::Dollar,         &S_PrefixParser}, //"$" 将int, 转为char

            {TokenType::Plus,           &S_PrefixParser}, //"+"
            {TokenType::Ampersand,      &S_PrefixParser}, // "&" substance accessing (ignores property operation)
            {TokenType::Asterisk,       &S_PrefixParser}, // "*" force property access
            // incontextof_expr "instanceof" unary_expr
            // incontextof_expr "in" unary_expr
//            {TokenType::Int,            &S_TypeCastPrefixParser}, // "int" unary_expr
//            {TokenType::Real,           &S_TypeCastPrefixParser}, // "real" unary_expr
//            {TokenType::String,         &S_TypeCastPrefixParser}, // "string" unary_expr
//            {TokenType::Function,       &S_FunctionPrefixParser}
    };

    /**
     * +----------------------------------------------------------------------------+
     * |                                InfixParser                                 |
     * +----------------------------------------------------------------------------+
     */

    class BinaryOperatorInfixParser final : public InfixParser {
    public:
        BinaryOperatorInfixParser(
                Precedence precedence,
                bool isRightAssociative,
                bool withAssignment = false) noexcept;

        ExprNode *parse(
                Result &r, Parser *parser,
                ExprNode *lhs, std::unique_ptr<Token> &token) override;

        [[nodiscard]] Precedence precedence() const override;

    private:
        Precedence _precedence;
        bool _withAssignment = false;
        bool _isRightAssociative = false;
    };

    static inline BinaryOperatorInfixParser S_SumSubBinOpInfixParser{
            Precedence::sum_sub, false
    };

    static inline BinaryOperatorInfixParser S_ProductBinOpParser{
            Precedence::product, false
    };

    static inline BinaryOperatorInfixParser S_AssignBinOpParser{
            Precedence::assignment, false, true
    };

    static inline BinaryOperatorInfixParser S_BitwiseShiftOrRollBinOpParser{
            Precedence::bitwise_shift_or_roll, false
    };

    static inline BinaryOperatorInfixParser S_BitwiseOrWithAssignBinOpParser{
            Precedence::bitwise_or, false, true
    };

    static inline BinaryOperatorInfixParser S_BitwiseAndWithAssignBinOpParser{
            Precedence::bitwise_and, false, true
    };

    static inline BinaryOperatorInfixParser S_BitwiseXorWithAssignBinOpParser{
            Precedence::bitwise_xor, false, true
    };

    static inline BinaryOperatorInfixParser S_EqualityBinOpParser{
            Precedence::equality, false
    };

    static inline BinaryOperatorInfixParser S_RelationalBinOpParser{
            Precedence::relational, false
    };

    static inline BinaryOperatorInfixParser S_LogicalOrBinOpParser{
            Precedence::logical_or, false
    };

    static inline BinaryOperatorInfixParser S_LogicalAndBinOpParser{
            Precedence::logical_and, false
    };

    static inline BinaryOperatorInfixParser S_MemberAccessBinOpParser{
            Precedence::postfix, false
    };

    static inline std::unordered_map<TokenType, InfixParser *> S_InfixParsers = {
            {TokenType::Swap,             &S_AssignBinOpParser}, // <->
            {TokenType::Assignment,       &S_AssignBinOpParser}, // =
            {TokenType::Plus,             &S_SumSubBinOpInfixParser}, // "+"
            {TokenType::PlusEqual,        &S_AssignBinOpParser}, // "+="
            {TokenType::Minus,            &S_SumSubBinOpInfixParser}, // "-"
            {TokenType::MinusEqual,       &S_AssignBinOpParser}, // "-="
            {TokenType::Slash,            &S_ProductBinOpParser}, // "/" 除法结果为浮点数
            {TokenType::SlashEqual,       &S_AssignBinOpParser}, // "/="
            {TokenType::Backslash,        &S_ProductBinOpParser}, // "\" 除法结果直接截断为整数
            {TokenType::BackslashEqual,   &S_AssignBinOpParser}, // "\="
            {TokenType::Percent,          &S_ProductBinOpParser}, // "%"
            {TokenType::PercentEqual,     &S_AssignBinOpParser}, // "%="
            {TokenType::Asterisk,         &S_ProductBinOpParser}, // "*"
            {TokenType::AsteriskEqual,    &S_AssignBinOpParser}, // "*="
            {TokenType::Exclamation,      &S_ProductBinOpParser}, // !

            {TokenType::LogicalAndEqual,  &S_AssignBinOpParser}, // "&&="
            {TokenType::LogicalOrEqual,   &S_AssignBinOpParser}, // "||="

            {TokenType::LArithShift,      &S_BitwiseShiftOrRollBinOpParser}, // "<<"
            {TokenType::LArithShiftEqual, &S_AssignBinOpParser}, // "<<="
            {TokenType::RArithShift,      &S_BitwiseShiftOrRollBinOpParser}, // ">>"
            {TokenType::RArithShiftEqual, &S_AssignBinOpParser}, // ">>="
            {TokenType::RBitShift,        &S_BitwiseShiftOrRollBinOpParser}, // ">>>"
            {TokenType::RBitShiftEqual,   &S_AssignBinOpParser}, // ">>>="

            {TokenType::Equal,            &S_EqualityBinOpParser}, // ==
            {TokenType::NotEqual,         &S_EqualityBinOpParser}, // !=
            {TokenType::DiscEqual,        &S_EqualityBinOpParser}, // ===
            {TokenType::DiscNotEqual,     &S_EqualityBinOpParser}, // !==

            {TokenType::Ampersand,        &S_BitwiseAndWithAssignBinOpParser}, // &
            {TokenType::VertLine,         &S_BitwiseOrWithAssignBinOpParser}, // |
            {TokenType::Chevron,          &S_BitwiseXorWithAssignBinOpParser}, // ^

            {TokenType::AmpersandEqual,   &S_AssignBinOpParser}, // "&="
            {TokenType::VertLineEqual,    &S_AssignBinOpParser}, // "|="
            {TokenType::ChevronEqual,     &S_AssignBinOpParser}, // "^="

            {TokenType::Lt,               &S_RelationalBinOpParser}, // <
            {TokenType::LtOrEqual,        &S_RelationalBinOpParser}, // <=
            {TokenType::Gt,               &S_RelationalBinOpParser}, // >=
            {TokenType::GtOrEqual,        &S_RelationalBinOpParser}, // >

            {TokenType::LogicalAnd,       &S_LogicalAndBinOpParser}, // &&
            {TokenType::LogicalOr,        &S_LogicalOrBinOpParser}, // ||

//            {TokenType::Question,         &S_ConditionalTernaryBinOpParser}, // cond ? expr : expr
            {TokenType::Dot,              &S_MemberAccessBinOpParser},
    };
}
