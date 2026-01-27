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
#include "common/SourceFile.hpp"
#include "parser/ast/AstBuilder.hpp"

namespace cial::syntax {
    using namespace Common;

    class Parser;

    enum class Precedence : u8 {
        lowest = 0,
        comma, // ,
        swap,
        conditional_ternary, // cond ? expr : expr
        assignment, // 一系列赋值语句
        keyword,
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
        type,
        variable,
    };

    class DeclParser {
    public:
        virtual ~DeclParser() = default;

        virtual DeclNode *parse(Result &r, Parser *parser, Token *token) const = 0;
    };

    class StmtParser {
    public:
        virtual ~StmtParser() = default;

        virtual StmtNode *parse(Result &r, Parser *parser, Token *token) const = 0;
    };

    class InfixParser {
    public:
        virtual ~InfixParser() = default;

        virtual ExprNode *parse(Result &r, Parser *parser, ExprNode *lhs, Token *token) const = 0;

        [[nodiscard]] virtual Precedence precedence() const = 0;
    };

    class PrefixParser {
    public:
        virtual ~PrefixParser() = default;

        virtual ExprNode *parse(Result &r, Parser *parser, Token *token) const = 0;
    };

    class Parser {
    public:
        explicit Parser(SourceFile &sourceFile, PreProcessor &preProcessor) :
            _lexer(Lexer{ sourceFile, preProcessor }), _sourceFile(sourceFile) {}

        void error(Result &r, const std::string &message, const SourceLocation &location) const {
            _sourceFile.error(r, message, location);
        }

        /**
         * Consume a token and remove it from the lexer.
         * Typically used with the `peek` method.
         *
         * @see peek
         * @return have token? if not return false, otherwise return true
         */
        bool consume(Result &r);

        /**
         * consume a token, will get token from lexer then pop from deque
         *
         * @return have token? if not return false, otherwise return true
         */
        bool consume(Result &r, Token &token);

        /**
         * Gets the current token at the top of the lexer's deque, returning a pointer.
         * WARN: Do not use this with the `consume` method!
         * WARN: Because the `consume` method removes the token.
         *
         * @return have token? if not return false, otherwise return true
         */
        bool current(Result &r, Token *&token);

        bool lookAhead(Result &r, size_t count);

        bool peek(Result &r, TokenType tokenType);

        void synchronize(Result &r);

        AstNode *parse(Result &r);

        bool parseScope(Result &r, BlockStmtNode *blockStmtNode, TokenType terminatorToken = TokenType::EndOfFile);

        DeclNode *parseDeclaration(Result &r);

        ExprNode *parseExpression(Result &r, bool enableCommaExpr, Precedence pre = Precedence::lowest);

        StmtNode *parseStatement(Result &r, bool enableCommaExpr);

        bool expect(Result &r, TokenType tokenType);

        [[nodiscard]] AstBuilder *astBuilder() { return &_astBuilder; }

    private:
        Lexer _lexer;
        AstBuilder _astBuilder{};
        SourceFile &_sourceFile;

        Precedence nextInfixPrecedence(Result &r, bool enableCommaExpr);

        static const DeclParser *declParserFor(TokenType type);

        static const StmtParser *stmtParserFor(TokenType type);

        static const InfixParser *infixParserFor(TokenType type, bool enableCommaExpr);

        static const PrefixParser *prefixParserFor(TokenType type);
    };

    /**
     * +----------------------------------------------------------------------------+
     * |                                declParser                                  |
     * +----------------------------------------------------------------------------+
     */
    struct PropertyDeclParser final : DeclParser {
        PropertyDeclParser() = default;

        DeclNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct VarDeclParser final : DeclParser {
        VarDeclParser() = default;

        DeclNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct FunctionDeclParser final : DeclParser {
        FunctionDeclParser() = default;

        DeclNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct ClassDeclParser final : DeclParser {
        ClassDeclParser() = default;

        DeclNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    static constinit PropertyDeclParser S_PropertyDeclParser{};
    static constinit VarDeclParser S_VarDeclParser{};
    static constinit FunctionDeclParser S_FunctionDeclParser{};
    static constinit ClassDeclParser S_ClassDeclParser{};

    static constinit auto S_DeclParsers =
        frozen::make_unordered_map<TokenType, const DeclParser *>({ { TokenType::Property, &S_PropertyDeclParser },
                                                                    { TokenType::Var, &S_VarDeclParser },
                                                                    { TokenType::Function, &S_FunctionDeclParser },
                                                                    { TokenType::Class, &S_ClassDeclParser } });

    /**
     * +----------------------------------------------------------------------------+
     * |                                stmtParser                                  |
     * +----------------------------------------------------------------------------+
     */
    struct BlockStmtParser final : StmtParser {
        BlockStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct TryStmtParser final : StmtParser {
        TryStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct IfStmtParser final : StmtParser {
        IfStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct SwitchStmtParser final : StmtParser {
        SwitchStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct DoWhileStmtParser final : StmtParser {
        DoWhileStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct ForStmtParser final : StmtParser {
        ForStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct WhileStmtParser final : StmtParser {
        WhileStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct BreakStmtParser final : StmtParser {
        BreakStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct ContinueStmtParser final : StmtParser {
        ContinueStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct ReturnStmtParser final : StmtParser {
        ReturnStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct DebuggerStmtParser final : StmtParser {
        DebuggerStmtParser() = default;
        StmtNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    static constinit BlockStmtParser S_BlockStmtParser{};
    static constinit TryStmtParser S_TryStmtParser{};
    static constinit IfStmtParser S_IfStmtParser{};
    static constinit SwitchStmtParser S_SwitchStmtParser{};
    static constinit DoWhileStmtParser S_DoWhileStmtParser{};
    static constinit ForStmtParser S_ForStmtParser{};
    static constinit WhileStmtParser S_WhileStmtParser{};
    static constinit BreakStmtParser S_BreakStmtParser{};
    static constinit ContinueStmtParser S_ContinueStmtParser{};
    static constinit ReturnStmtParser S_ReturnStmtParser{};
    static constinit DebuggerStmtParser S_DebuggerStmtParser{};

    static constinit auto S_StmtParsers =
        frozen::make_unordered_map<TokenType, const StmtParser *>({ { TokenType::LeftCurlyBrace, &S_BlockStmtParser },
                                                                    { TokenType::Try, &S_TryStmtParser },
                                                                    { TokenType::If, &S_IfStmtParser },
                                                                    { TokenType::Switch, &S_SwitchStmtParser },
                                                                    { TokenType::Do, &S_DoWhileStmtParser },
                                                                    { TokenType::For, &S_ForStmtParser },
                                                                    { TokenType::While, &S_WhileStmtParser },
                                                                    { TokenType::Break, &S_BreakStmtParser },
                                                                    { TokenType::Continue, &S_ContinueStmtParser },
                                                                    { TokenType::Return, &S_ReturnStmtParser },
                                                                    { TokenType::Debugger, &S_DebuggerStmtParser } });

    /**
     * +----------------------------------------------------------------------------+
     * |                                prefixParser                                |
     * +----------------------------------------------------------------------------+
     */

    struct ConstValPrefixParser final : PrefixParser {
        ConstValPrefixParser() = default;

        ExprNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct UnaryOperatorPrefixParser final : PrefixParser {
        constexpr explicit UnaryOperatorPrefixParser(const Precedence precedence) noexcept : _precedence(precedence) {}

        ExprNode *parse(Result &r, Parser *parser, Token *token) const override;

    private:
        Precedence _precedence;
    };

    struct IdentifierPrefixParser final : PrefixParser {
        IdentifierPrefixParser() = default;

        ExprNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct InternalIdentifierPrefixParser final : PrefixParser {
        InternalIdentifierPrefixParser() = default;

        ExprNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct ParenthesizedPrefixParser final : PrefixParser {
        ParenthesizedPrefixParser() = default;

        ExprNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    struct FunctionPrefixParser final : PrefixParser {
        FunctionPrefixParser() = default;

        ExprNode *parse(Result &r, Parser *parser, Token *token) const override;
    };

    static constinit ConstValPrefixParser S_ConstValPrefixParser{};
    static constinit UnaryOperatorPrefixParser S_NegatePrefixParser{ Precedence::sum_sub };
    static constinit IdentifierPrefixParser S_IdentifierPrefixParser;
    static constinit UnaryOperatorPrefixParser S_PrefixParser{ Precedence::prefix };
    static constinit InternalIdentifierPrefixParser S_InternalIdentifierPrefixParser;
    static constinit ParenthesizedPrefixParser S_ParenthesizedPrefixParser{};
    static constinit FunctionPrefixParser S_FunctionPrefixParser;
    static constinit UnaryOperatorPrefixParser S_TypeCastPrefixParser{ Precedence::type };

    static constinit auto S_PrefixParsers = frozen::make_unordered_map<TokenType, const PrefixParser *>(
        { { TokenType::Null, &S_ConstValPrefixParser },
          { TokenType::ConstVal, &S_ConstValPrefixParser },
          { TokenType::Minus, &S_NegatePrefixParser }, // "-"
          { TokenType::Identifier, &S_IdentifierPrefixParser },
          { TokenType::Exclamation, &S_PrefixParser }, // "!"
          { TokenType::Tilde, &S_PrefixParser }, // "~"
          { TokenType::Decrement, &S_PrefixParser }, // "--"
          { TokenType::Increment, &S_PrefixParser }, // "++"
          { TokenType::New, &S_PrefixParser }, // "new" 函数调用, 或创建新对象
          { TokenType::Global, &S_InternalIdentifierPrefixParser },
          { TokenType::Super, &S_InternalIdentifierPrefixParser },
          { TokenType::This, &S_InternalIdentifierPrefixParser },
          { TokenType::Invalidate, &S_PrefixParser }, // "invalidate"
          { TokenType::Isvalid, &S_PrefixParser }, // "isvalid"
          { TokenType::Delete, &S_PrefixParser }, //"delete"
          { TokenType::Typeof, &S_PrefixParser }, //"typeof
          { TokenType::Sharp, &S_PrefixParser }, //"#" 获取字符串第一个字符,转为int
          { TokenType::Dollar, &S_PrefixParser }, //"$" 将int, 转为char
          { TokenType::Plus, &S_PrefixParser }, //"+"
          { TokenType::Ampersand, &S_PrefixParser }, // "&" substance accessing (ignores property operation)
          { TokenType::Asterisk, &S_PrefixParser }, // "*" force property access
          { TokenType::LParenthesis, &S_ParenthesizedPrefixParser }, // "(" 括号表达式
          { TokenType::Function, &S_FunctionPrefixParser },
          { TokenType::Int, &S_TypeCastPrefixParser }, // "int" unary_expr
          { TokenType::Real, &S_TypeCastPrefixParser }, // "real" unary_expr
          { TokenType::String, &S_TypeCastPrefixParser }, // "string" unary_expr
          { TokenType::Throw, &S_PrefixParser } });

    /**
     * +----------------------------------------------------------------------------+
     * |                                InfixParser                                 |
     * +----------------------------------------------------------------------------+
     */

    struct BinaryOperatorInfixParser final : InfixParser {
        constexpr explicit BinaryOperatorInfixParser(const Precedence precedence, const bool isRightAssociative,
                                                     const bool withAssignment = false) :
            _precedence(precedence), _withAssignment(withAssignment), _isRightAssociative(isRightAssociative) {}


        ExprNode *parse(Result &r, Parser *parser, ExprNode *lhs, Token *token) const override;

        [[nodiscard]] Precedence precedence() const override { return _precedence; }

    private:
        const Precedence _precedence;
        const bool _withAssignment;
        const bool _isRightAssociative;
    };

    struct ProcCallInfixParser final : InfixParser {
        explicit ProcCallInfixParser() = default;

        ExprNode *parse(Result &r, Parser *parser, ExprNode *lhs, Token *token) const override;

        [[nodiscard]] Precedence precedence() const override { return Precedence::postfix; }
    };

    struct TernaryInfixParser final : InfixParser {
        explicit TernaryInfixParser() = default;

        ExprNode *parse(Result &r, Parser *parser, ExprNode *lhs, Token *token) const override;

        [[nodiscard]] Precedence precedence() const override { return Precedence::postfix; }
    };

    struct UnaryInfixParser final : InfixParser {
        explicit UnaryInfixParser() = default;

        ExprNode *parse(Result &r, Parser *parser, ExprNode *lhs, Token *token) const override;

        [[nodiscard]] Precedence precedence() const override { return Precedence::postfix; }
    };

    static constinit BinaryOperatorInfixParser S_SumSubBinOpInfixParser{ Precedence::sum_sub, false },
        S_ProductBinOpParser{ Precedence::product, false }, S_AssignBinOpParser{ Precedence::assignment, true, true },
        S_BitwiseShiftOrRollBinOpParser{ Precedence::bitwise_shift_or_roll, false },
        S_BitwiseOrWithAssignBinOpParser{ Precedence::bitwise_or, false, true },
        S_BitwiseAndWithAssignBinOpParser{ Precedence::bitwise_and, false, true },
        S_BitwiseXorWithAssignBinOpParser{ Precedence::bitwise_xor, false, true },
        S_EqualityBinOpParser{ Precedence::equality, false }, S_RelationalBinOpParser{ Precedence::relational, false },
        S_LogicalOrBinOpParser{ Precedence::logical_or, false }, S_SwapBinOpParser{ Precedence::swap, false },
        S_LogicalAndBinOpParser{ Precedence::logical_and, false }, S_OrderBinOpParser{ Precedence::comma, false },
        S_PostfixBinOpParser{ Precedence::postfix, false }, S_KeywordInfixBinOpParser{ Precedence::keyword, false };

    static constinit ProcCallInfixParser S_ProcCallInfixParser{};
    static constinit TernaryInfixParser S_ConditionalTernaryBinOpParser{};
    static constinit UnaryInfixParser S_UnaryInfixParser{};

    static constinit auto S_InfixParsers = frozen::make_unordered_map<TokenType, const InfixParser *>({
        { TokenType::Comma, &S_OrderBinOpParser }, // ,
        { TokenType::Swap, &S_SwapBinOpParser }, // <->
        { TokenType::Assignment, &S_AssignBinOpParser }, // =
        { TokenType::Plus, &S_SumSubBinOpInfixParser }, // "+"
        { TokenType::PlusEqual, &S_AssignBinOpParser }, // "+="
        { TokenType::Minus, &S_SumSubBinOpInfixParser }, // "-"
        { TokenType::MinusEqual, &S_AssignBinOpParser }, // "-="
        { TokenType::Slash, &S_ProductBinOpParser }, // "/" 除法结果为浮点数
        { TokenType::SlashEqual, &S_AssignBinOpParser }, // "/="
        { TokenType::Backslash, &S_ProductBinOpParser }, // "\" 除法结果直接截断为整数
        { TokenType::BackslashEqual, &S_AssignBinOpParser }, // "\="
        { TokenType::Percent, &S_ProductBinOpParser }, // "%"
        { TokenType::PercentEqual, &S_AssignBinOpParser }, // "%="
        { TokenType::Asterisk, &S_ProductBinOpParser }, // "*"
        { TokenType::AsteriskEqual, &S_AssignBinOpParser }, // "*="
        { TokenType::Exclamation, &S_ProductBinOpParser }, // !
        { TokenType::LogicalAndEqual, &S_AssignBinOpParser }, // "&&="
        { TokenType::LogicalOrEqual, &S_AssignBinOpParser }, // "||="
        { TokenType::LArithShift, &S_BitwiseShiftOrRollBinOpParser }, // "<<"
        { TokenType::LArithShiftEqual, &S_AssignBinOpParser }, // "<<="
        { TokenType::RArithShift, &S_BitwiseShiftOrRollBinOpParser }, // ">>"
        { TokenType::RArithShiftEqual, &S_AssignBinOpParser }, // ">>="
        { TokenType::RBitShift, &S_BitwiseShiftOrRollBinOpParser }, // ">>>"
        { TokenType::RBitShiftEqual, &S_AssignBinOpParser }, // ">>>="
        { TokenType::Equal, &S_EqualityBinOpParser }, // ==
        { TokenType::NotEqual, &S_EqualityBinOpParser }, // !=
        { TokenType::DiscEqual, &S_EqualityBinOpParser }, // ===
        { TokenType::DiscNotEqual, &S_EqualityBinOpParser }, // !==
        { TokenType::Ampersand, &S_BitwiseAndWithAssignBinOpParser }, // &
        { TokenType::VertLine, &S_BitwiseOrWithAssignBinOpParser }, // |
        { TokenType::Chevron, &S_BitwiseXorWithAssignBinOpParser }, // ^
        { TokenType::AmpersandEqual, &S_AssignBinOpParser }, // "&="
        { TokenType::VertLineEqual, &S_AssignBinOpParser }, // "|="
        { TokenType::ChevronEqual, &S_AssignBinOpParser }, // "^="
        { TokenType::Lt, &S_RelationalBinOpParser }, // <
        { TokenType::LtOrEqual, &S_RelationalBinOpParser }, // <=
        { TokenType::Gt, &S_RelationalBinOpParser }, // >=
        { TokenType::GtOrEqual, &S_RelationalBinOpParser }, // >
        { TokenType::LogicalAnd, &S_LogicalAndBinOpParser }, // &&
        { TokenType::LogicalOr, &S_LogicalOrBinOpParser }, // ||
        { TokenType::Dot, &S_PostfixBinOpParser }, // .
        { TokenType::InContextOf, &S_KeywordInfixBinOpParser }, // "incontextof"
        { TokenType::Instanceof, &S_KeywordInfixBinOpParser }, // "instanceof"
        { TokenType::Isvalid, &S_UnaryInfixParser }, // "isvalid"
        { TokenType::Decrement, &S_UnaryInfixParser }, // "--"
        { TokenType::Increment, &S_UnaryInfixParser }, // "++"
        { TokenType::Question, &S_ConditionalTernaryBinOpParser }, // cond ? expr : expr
        { TokenType::LParenthesis, &S_ProcCallInfixParser } // ()
    });
} // namespace cial::syntax
