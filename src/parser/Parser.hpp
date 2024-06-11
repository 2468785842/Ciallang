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

#include "../lexer/Lexer.hpp"
#include "../common/SourceFile.hpp"
#include "../ast/Ast.hpp"

namespace Ciallang::Syntax {
    using namespace Common;

    class Parser;

    enum class Precedence : uint8_t {
        lowest = 0,
        comma,               // ,
        assignment,          // 一系列赋值语句
        conditional_ternary, // cond ? expr : expr
        // key_value,
        logical_or,            // ||
        logical_and,           // &&
        bitwise_or,            // |
        bitwise_xor,           // ^
        bitwise_and,           // &
        equality,              // == !=
        relational,            // > >= < <=
        bitwise_shift_or_roll, // >> << >>>
        sum_sub,               // + -
        product,               // * / \ %
        prefix,                // - ~ ++ -- new
        postfix,               // [] () .
        cast,                  //
        type,
        variable,
        call
    };

    class DeclParser {
    public:
        virtual ~DeclParser() = default;

        virtual DeclNode* parse(Result& r, Parser* parser,
                                Token* token) const = 0;
    };

    class StmtParser {
    public:
        virtual ~StmtParser() = default;

        virtual StmtNode* parse(Result& r, Parser* parser,
                                Token* token) const = 0;
    };

    class InfixParser {
    public:
        virtual ~InfixParser() = default;

        virtual ExprNode* parse(Result& r, Parser* parser,
                                ExprNode* lhs, Token* token) const = 0;

        // 优先级
        [[nodiscard]] virtual Precedence precedence() const = 0;
    };

    class PrefixParser {
    public:
        virtual ~PrefixParser() = default;

        virtual ExprNode* parse(Result& r, Parser* parser,
                                Token* token) const = 0;
    };

    class Parser {
    public:
        Parser(SourceFile& sourceFile, AstBuilder& builder) :
            _lexer(Lexer{ sourceFile }), _astBuilder(builder),
            _sourceFile(sourceFile) {
        }

        void error(
            Result& r,
            const string& message,
            const SourceLocation& location) const {
            _sourceFile.error(r, message, location);
        }

        bool consume();

        bool consume(Token& token);

        bool current(Token&);

        bool lookAhead(size_t count);

        bool peek(TokenType);

        void synchronize();

        AstNode* parse(Result&);

        void parseScope(Result&, BlockStmtNode*, TokenType = TokenType::EndOfFile);

        DeclNode* parseDeclaration(Result& r);

        ExprNode* parseExpression(Result&, Precedence = Precedence::lowest);

        StmtNode* parseStatement(Result&);

        static void writeAstGraph(const filesystem::path&, AstNode* programNode);

        bool expect(Result&, const Token*);

        [[nodiscard]] AstBuilder* astBuilder() const { return &_astBuilder; }

    private:
        Lexer _lexer;
        AstBuilder& _astBuilder;
        SourceFile& _sourceFile;

        Precedence currentInfixPrecedence();

        static const DeclParser* declParserFor(TokenType type);

        static const StmtParser* stmtParserFor(TokenType type);

        static const InfixParser* infixParserFor(TokenType type);

        static const PrefixParser* prefixParserFor(TokenType type);

        [[nodiscard]] constexpr const std::vector<Token*>& tokens() const {
            return _lexer.tokens();
        }
    };

    /**
     * +----------------------------------------------------------------------------+
     * |                                declParser                                  |
     * +----------------------------------------------------------------------------+
     */
    struct VarDeclParser final : DeclParser {
        constexpr VarDeclParser() = default;

        DeclNode* parse(Result& r, Parser* parser, Token* token) const override;
    };

    struct FunctionDeclParser final : DeclParser {
        constexpr FunctionDeclParser() = default;

        DeclNode* parse(Result& r, Parser* parser, Token* token) const override;
    };

    static constexpr VarDeclParser S_VarDeclParser{};
    static constexpr FunctionDeclParser S_FunctionDeclParser{};

    static constexpr auto S_DeclParsers =
            frozen::make_unordered_map<TokenType, const DeclParser*>({
                    { TokenType::Var, &S_VarDeclParser },
                    { TokenType::Function, &S_FunctionDeclParser }
            });

    /**
     * +----------------------------------------------------------------------------+
     * |                                stmtParser                                  |
     * +----------------------------------------------------------------------------+
     */
    struct BlockStmtParser final : StmtParser {
        constexpr BlockStmtParser() = default;

        StmtNode* parse(Result& r, Parser* parser, Token* token) const override;
    };

    struct IfStmtParser final : StmtParser {
        constexpr IfStmtParser() = default;

        StmtNode* parse(Result& r, Parser* parser, Token* token) const override;
    };

    struct WhileStmtParser final : StmtParser {
        constexpr WhileStmtParser() = default;
        StmtNode* parse(Result& r, Parser* parser, Token* token) const override;
    };

    struct BreakStmtParser final : StmtParser {
        constexpr BreakStmtParser() = default;
        StmtNode* parse(Result& r, Parser* parser, Token* token) const override;
    };

    struct ContinueStmtParser final : StmtParser {
        constexpr ContinueStmtParser() = default;
        StmtNode* parse(Result& r, Parser* parser, Token* token) const override;
    };

    struct ReturnStmtParser final : StmtParser {
        constexpr ReturnStmtParser() = default;
        StmtNode* parse(Result& r, Parser* parser, Token* token) const override;
    };

    static constexpr BlockStmtParser S_BlockStmtParser{};
    static constexpr IfStmtParser S_IfStmtParser{};
    static constexpr WhileStmtParser S_WhileStmtParser{};
    static constexpr BreakStmtParser S_BreakStmtParser{};
    static constexpr ContinueStmtParser S_ContinueStmtParser{};
    static constexpr ReturnStmtParser S_ReturnStmtParser{};

    static constexpr auto S_StmtParsers =
            frozen::make_unordered_map<TokenType, const StmtParser*>({
                    { TokenType::LeftCurlyBrace, &S_BlockStmtParser },
                    { TokenType::If, &S_IfStmtParser },
                    { TokenType::While, &S_WhileStmtParser },
                    { TokenType::Break, &S_BreakStmtParser },
                    { TokenType::Continue, &S_ContinueStmtParser },
                    { TokenType::Return, &S_ReturnStmtParser },
            });

    /**
     * +----------------------------------------------------------------------------+
     * |                                prefixParser                                |
     * +----------------------------------------------------------------------------+
     */

    struct ConstValPrefixParser final : PrefixParser {
        constexpr ConstValPrefixParser() = default;

        ExprNode* parse(Result& r, Parser* parser, Token* token) const override;
    };

    struct UnaryOperatorPrefixParser final : PrefixParser {
        constexpr explicit UnaryOperatorPrefixParser(
            const Precedence precedence) noexcept: _precedence(precedence) {
        }

        ExprNode* parse(Result& r, Parser* parser, Token* token) const override;

    private:
        Precedence _precedence;
    };

    struct SymbolPrefixParser final : PrefixParser {
        constexpr SymbolPrefixParser() = default;

        ExprNode* parse(Result& r, Parser* parser, Token* token) const override;
    };

    //    struct FunctionPrefixParser final : public PrefixParser {
    //        FunctionPrefixParser() = default;
    //
    //        ExprNode *parse(Result &r, Parser *parser, Token *token) override;
    //    };

    static constexpr ConstValPrefixParser S_ConstValPrefixParser{};
    static constexpr UnaryOperatorPrefixParser S_NegatePrefixParser{ Precedence::sum_sub };
    static constexpr SymbolPrefixParser S_SymbolPrefixParser;
    static constexpr UnaryOperatorPrefixParser S_PrefixParser{ Precedence::prefix };
    //    static inline FunctionPrefixParser S_FunctionPrefixParser{};

    static constexpr auto S_PrefixParsers =
            frozen::make_unordered_map<TokenType, const PrefixParser*>({
                    // { TokenType::Const, &S_PrefixParser },
                    { TokenType::ConstVal, &S_ConstValPrefixParser },
                    { TokenType::Minus, &S_NegatePrefixParser }, // "-"
                    { TokenType::Identifier, &S_SymbolPrefixParser },
                    { TokenType::Exclamation, &S_PrefixParser }, // "!"
                    { TokenType::Tilde, &S_PrefixParser },       // "~"
                    { TokenType::Decrement, &S_PrefixParser },   // "--"
                    { TokenType::Increment, &S_PrefixParser },   // "++"
                    { TokenType::New, &S_PrefixParser },         // "new" 函数调用, 或创建新对象
                    { TokenType::Invalidate, &S_PrefixParser },  // "invalidate"
                    { TokenType::Isvalid, &S_PrefixParser },     // "isvalid" todo: 注意还有中缀表示
                    // incontextof_expr "isvalid"
                    { TokenType::Delete, &S_PrefixParser },    //"delete"
                    { TokenType::Typeof, &S_PrefixParser },    //"typeof
                    { TokenType::Sharp, &S_PrefixParser },     //"#" 获取字符串第一个字符,转为int
                    { TokenType::Dollar, &S_PrefixParser },    //"$" 将int, 转为char
                    { TokenType::Plus, &S_PrefixParser },      //"+"
                    { TokenType::Ampersand, &S_PrefixParser }, // "&" substance accessing (ignores property operation)
                    { TokenType::Asterisk, &S_PrefixParser },  // "*" force property access
                    // incontextof_expr "instanceof" unary_expr
                    // incontextof_expr "in" unary_expr
                    // {TokenType::Int,            &S_TypeCastPrefixParser}, // "int" unary_expr
                    // {TokenType::Real,           &S_TypeCastPrefixParser}, // "real" unary_expr
                    // {TokenType::String,         &S_TypeCastPrefixParser}, // "string" unary_expr
                    // {TokenType::Function,       &S_FunctionPrefixParser}
            });

    /**
     * +----------------------------------------------------------------------------+
     * |                                InfixParser                                 |
     * +----------------------------------------------------------------------------+
     */

    struct BinaryOperatorInfixParser final : InfixParser {
        constexpr explicit BinaryOperatorInfixParser(
            const Precedence precedence,
            const bool isRightAssociative,
            const bool withAssignment = false
        ) noexcept: _precedence(precedence),
                    _withAssignment(withAssignment),
                    _isRightAssociative(isRightAssociative) {
        }


        ExprNode* parse(Result& r, Parser* parser,
                        ExprNode* lhs, Token* token) const override;

        [[nodiscard]] constexpr Precedence precedence() const override {
            return _precedence;
        }

    private:
        const Precedence _precedence;
        const bool _withAssignment;
        const bool _isRightAssociative;
    };

    struct ProcCallInfixParser final : InfixParser {
        constexpr explicit ProcCallInfixParser() = default;

        ExprNode* parse(Result& r, Parser* parser,
                        ExprNode* lhs, Token* token) const override;

        [[nodiscard]] constexpr Precedence precedence() const override {
            return Precedence::call;
        }
    };

    static constexpr BinaryOperatorInfixParser
            S_SumSubBinOpInfixParser{ Precedence::sum_sub, false },
            S_ProductBinOpParser{ Precedence::product, false },
            S_AssignBinOpParser{ Precedence::assignment, true, true },
            S_BitwiseShiftOrRollBinOpParser{ Precedence::bitwise_shift_or_roll, false },
            S_BitwiseOrWithAssignBinOpParser{ Precedence::bitwise_or, false, true },
            S_BitwiseAndWithAssignBinOpParser{ Precedence::bitwise_and, false, true },
            S_BitwiseXorWithAssignBinOpParser{ Precedence::bitwise_xor, false, true },
            S_EqualityBinOpParser{ Precedence::equality, false },
            S_RelationalBinOpParser{ Precedence::relational, false },
            S_LogicalOrBinOpParser{ Precedence::logical_or, false },
            S_LogicalAndBinOpParser{ Precedence::logical_and, false },
            S_MemberAccessBinOpParser{ Precedence::postfix, false };

    static constexpr ProcCallInfixParser S_ProcCallInfixParser{};

    static constexpr auto S_InfixParsers =
            frozen::make_unordered_map<TokenType, const InfixParser*>({
                    { TokenType::Swap, &S_AssignBinOpParser },                    // <->
                    { TokenType::Assignment, &S_AssignBinOpParser },              // =
                    { TokenType::Plus, &S_SumSubBinOpInfixParser },               // "+"
                    { TokenType::PlusEqual, &S_AssignBinOpParser },               // "+="
                    { TokenType::Minus, &S_SumSubBinOpInfixParser },              // "-"
                    { TokenType::MinusEqual, &S_AssignBinOpParser },              // "-="
                    { TokenType::Slash, &S_ProductBinOpParser },                  // "/" 除法结果为浮点数
                    { TokenType::SlashEqual, &S_AssignBinOpParser },              // "/="
                    { TokenType::Backslash, &S_ProductBinOpParser },              // "\" 除法结果直接截断为整数
                    { TokenType::BackslashEqual, &S_AssignBinOpParser },          // "\="
                    { TokenType::Percent, &S_ProductBinOpParser },                // "%"
                    { TokenType::PercentEqual, &S_AssignBinOpParser },            // "%="
                    { TokenType::Asterisk, &S_ProductBinOpParser },               // "*"
                    { TokenType::AsteriskEqual, &S_AssignBinOpParser },           // "*="
                    { TokenType::Exclamation, &S_ProductBinOpParser },            // !
                    { TokenType::LogicalAndEqual, &S_AssignBinOpParser },         // "&&="
                    { TokenType::LogicalOrEqual, &S_AssignBinOpParser },          // "||="
                    { TokenType::LArithShift, &S_BitwiseShiftOrRollBinOpParser }, // "<<"
                    { TokenType::LArithShiftEqual, &S_AssignBinOpParser },        // "<<="
                    { TokenType::RArithShift, &S_BitwiseShiftOrRollBinOpParser }, // ">>"
                    { TokenType::RArithShiftEqual, &S_AssignBinOpParser },        // ">>="
                    { TokenType::RBitShift, &S_BitwiseShiftOrRollBinOpParser },   // ">>>"
                    { TokenType::RBitShiftEqual, &S_AssignBinOpParser },          // ">>>="
                    { TokenType::Equal, &S_EqualityBinOpParser },                 // ==
                    { TokenType::NotEqual, &S_EqualityBinOpParser },              // !=
                    { TokenType::DiscEqual, &S_EqualityBinOpParser },             // ===
                    { TokenType::DiscNotEqual, &S_EqualityBinOpParser },          // !==
                    { TokenType::Ampersand, &S_BitwiseAndWithAssignBinOpParser }, // &
                    { TokenType::VertLine, &S_BitwiseOrWithAssignBinOpParser },   // |
                    { TokenType::Chevron, &S_BitwiseXorWithAssignBinOpParser },   // ^
                    { TokenType::AmpersandEqual, &S_AssignBinOpParser },          // "&="
                    { TokenType::VertLineEqual, &S_AssignBinOpParser },           // "|="
                    { TokenType::ChevronEqual, &S_AssignBinOpParser },            // "^="
                    { TokenType::Lt, &S_RelationalBinOpParser },                  // <
                    { TokenType::LtOrEqual, &S_RelationalBinOpParser },           // <=
                    { TokenType::Gt, &S_RelationalBinOpParser },                  // >=
                    { TokenType::GtOrEqual, &S_RelationalBinOpParser },           // >
                    { TokenType::LogicalAnd, &S_LogicalAndBinOpParser },          // &&
                    { TokenType::LogicalOr, &S_LogicalOrBinOpParser },            // ||
                    //            {TokenType::Question,         &S_ConditionalTernaryBinOpParser}, // cond ? expr : expr
                    { TokenType::Dot, &S_MemberAccessBinOpParser },
                    { TokenType::LParenthesis, &S_ProcCallInfixParser }
            });
}
