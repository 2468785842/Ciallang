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

#include "Parser.hpp"

#include "logging/Logger.hpp"
#include "parser/ast/DeclNode.hpp"
#include "parser/ast/ExprNode.hpp"
#include "parser/ast/StmtNode.hpp"

namespace cial::Syntax {
    using namespace Common;

    static ExprNode *createExpressionNode(Result &r, Parser *parser) {
        // expect "("
        if(!parser->expect(r, TokenType::LParenthesis))
            return nullptr;

        const auto node = parser->parseExpression(r);

        // expect ")"
        if(!parser->expect(r, TokenType::RParenthesis))
            return nullptr;
        return node;
    }

    /**
     * NOTE: This language allows commas to represent implicit 'void' arguments.
     * e.g.
     * a(,) -> a(void, void)
     * a(,,) -> a(void, void, void)
     * a(2,) -> a(2, void)
     * a(,2) -> a(void, 2)
     */
    static bool parseArguments(Result &r, Parser *parser, ProcCallExprNode *node) {
        bool expectArgument = true;

        while(!parser->peek(TokenType::RParenthesis)) {
            if(expectArgument) {
                // 检查当前token是否为逗号
                if(parser->peek(TokenType::Comma)) {
                    // 遇到逗号，添加隐式void参数
                    node->arguments.push_back(parser->astBuilder()->makeNode<ValueExprNode>(Token{}));

                    // 期望并消耗逗号
                    if(!parser->expect(r, TokenType::Comma))
                        return false;
                    // 继续期望参数
                    expectArgument = true;
                } else {
                    // 解析表达式参数
                    auto *expr = parser->parseExpression(r);
                    if(!expr)
                        return false;
                    node->arguments.push_back(expr);
                    expectArgument = false;
                }
            } else {
                // 期望逗号
                if(!parser->expect(r, TokenType::Comma))
                    return false;
                expectArgument = true;
            }
        }

        // 处理尾随逗号后的隐式void参数
        // 只有在expectArgument为true时才添加void参数
        if(expectArgument) {
            node->arguments.push_back(parser->astBuilder()->makeNode<ValueExprNode>(Token{}));
        }

        return true;
    }

    static bool parseParameters(Result &r, Parser *parser, FunctionDeclNode *funNode) {
        if(!parser->expect(r, TokenType::LParenthesis))
            return false;

        if(parser->peek(TokenType::RParenthesis)) {
            parser->consume();
            return true;
        }

        for(;;) {
            if(!parser->peek(TokenType::Identifier)) {
                parser->error(r, "function parameter expect a identifier", funNode->location);
                return false;
            }
            Token identifier{};
            parser->current(identifier);
            parser->consume();


            // default value
            ExprNode *expr = nullptr;
            if(parser->peek(TokenType::Assignment)) {
                parser->consume();
                Token assignmentToken{};
                parser->current(assignmentToken);
                expr = parser->parseExpression(r);
                if(!expr)
                    return false;
            }

            funNode->parameters.emplace_back(identifier, expr);

            if(parser->peek(TokenType::RParenthesis)) {
                parser->consume();
                break;
            }

            if(!parser->expect(r, TokenType::Comma))
                return false;
        }

        return true;
    }

    bool Parser::lookAhead(const size_t count) {
        while(count >= tokens().size() && _lexer.hasNext()) {
            Token *token{ nullptr };
            if(!_lexer.next(token))
                break;

            if(token->type() == TokenType::LineComment || token->type() == TokenType::BlockComment) {
                _lexer.tackOverToken(*token);
            }

            CLL_ASSERT(token != nullptr, "token is null");
        }
        return !tokens().empty();
    }

    bool Parser::peek(const TokenType tokenType) {
        if(!lookAhead(0))
            return false;
        const auto &token = tokens().front();
        return token->type() == tokenType;
    }

    bool Parser::consume() {
        Token token{};
        return consume(token);
    }

    bool Parser::consume(Token &token) {
        if(!lookAhead(0))
            return false;

        return _lexer.tackOverToken(token);
    }

    bool Parser::current(Token &token) {
        // just check tokens is empty? current tokens is empty we lex,
        if(!lookAhead(0))
            return false;

        token = *tokens().front();

        return token.type() != TokenType::EndOfFile;
    }

    bool Parser::expect(Result &r, const TokenType tokenType) {
        if(!lookAhead(0))
            return false;

        std::string expectedName = tokenTypeToStr(tokenType);
        Token tToken{};

        if(!_lexer.tackOverToken(tToken)) {
            error(r, fmt::format("expected token '{}' but end of file.", expectedName, tToken.name()), tToken.location);
            return false;
        }

        if(tToken.type() != tokenType) {
            error(r, fmt::format("expected token '{}' but found '{}'.", expectedName, tToken.name()), tToken.location);
            return false;
        }

        return true;
    }

    /**
     * 获取下一个Token优先级
     * @return Token优先级
     */
    Precedence Parser::nextInfixPrecedence() {
        if(lookAhead(0)) {
            const auto *token = tokens().front();
            if(const auto infixParser = infixParserFor(token->type()))
                return infixParser->precedence();
        }
        return Precedence::lowest;
    }

    void Parser::synchronize() {
        while(!peek(TokenType::EndOfFile)) {
            Token token{};
            if(!current(token))
                return;
            if(token.type() == TokenType::Invalid)
                return;

            switch(token.type()) {
                case TokenType::Function:
                case TokenType::Class:
                case TokenType::Var:
                case TokenType::Const:
                case TokenType::For:
                case TokenType::If:
                case TokenType::While:
                case TokenType::Return:
                    return;
                default:
                    if(!consume())
                        return;
            }
        }
    }

    AstNode *Parser::parse(Result &r) {
        auto *global = _astBuilder.makeNode<BlockStmtNode>();
        parseScope(r, global);
        return global;
    }

    void Parser::parseScope(Result &r, BlockStmtNode *blockStmtNode, const TokenType terminatorToken) {
        while(_lexer.hasNext()) {
            if(peek(terminatorToken))
                return;
            auto *statement = parseDeclaration(r);

            // error sync
            if(!statement) {
                synchronize();
                continue;
            }

            blockStmtNode->childrens.push_back(statement);
        }
    }

    DeclNode *Parser::parseDeclaration(Result &r) {
        Token token{};
        if(!current(token))
            return nullptr;

        if(const auto *declParser = declParserFor(token.type())) {
            consume();
            return declParser->parse(r, this, &token);
        }

        if(const auto *stmt = parseStatement(r)) {
            return _astBuilder.makeNode<StmtDeclNode>(stmt);
        }

        return nullptr;
    }

    ExprNode *Parser::parseExpression(Result &r, const Precedence pre) {
        Token token{};
        if(!consume(token))
            return nullptr;

        // 前缀
        const auto *prefixParser = prefixParserFor(token.type());

        if(!prefixParser) {
            error(r, fmt::format("unable prefix parse for token '{}'.", token.name()), token.location);
            return nullptr;
        }

        ExprNode *lhs = prefixParser->parse(r, this, &token);
        if(!lhs) {
            error(r, "unexpected empty ast node.", token.location);
            return nullptr;
        }

        // 中缀
        while(pre < nextInfixPrecedence()) {
            if(!consume(token))
                break;

            const auto infixParser = infixParserFor(token.type());
            if(infixParser == nullptr) {
                error(r, fmt::format("unable infix parse for token '{}' not found parser.", token.name()),
                      token.location);
                break;
            }

            lhs = infixParser->parse(r, this, lhs, &token);

            if(lhs == nullptr || r.isFailed())
                break;
        }
        return lhs;
    }

    /**
     * 解析代码块中的语句
     * if(a > b){
     *      var a = 1; <- parseStatement
     *      ...more    <- parseStatement
     *      var b = a; <- parseStatement
     * }
     */
    StmtNode *Parser::parseStatement(Result &r) {
        Token token{};

        // just peek
        if(!current(token))
            return nullptr;

        if(const auto stmtParser = stmtParserFor(token.type())) {
            consume();
            return stmtParser->parse(r, this, &token);
        }

        // maybe ExpressionStatement

        if(const auto *expr = parseExpression(r)) {
            auto *statementNode = _astBuilder.makeNode<ExprStmtNode>(expr);
            statementNode->location = expr->location;

            // ;
            if(expect(r, TokenType::SemiColon))
                return statementNode;
        }

        return nullptr;
    }

    const DeclParser *Parser::declParserFor(const TokenType type) {
        const auto it = S_DeclParsers.find(type);
        return it != S_DeclParsers.end() ? it->second : nullptr;
    }

    const StmtParser *Parser::stmtParserFor(const TokenType type) {
        const auto it = S_StmtParsers.find(type);
        return it != S_StmtParsers.end() ? it->second : nullptr;
    }

    /**
     * 尝试获取一个前缀表达式Token的解析器
     *
     * @param type has TokenParser -> TokenParser
     *             else -> nullptr
     * @return Token解析器
     */
    const PrefixParser *Parser::prefixParserFor(const TokenType type) {
        const auto it = S_PrefixParsers.find(type);
        return it != S_PrefixParsers.end() ? it->second : nullptr;
    }

    /**
     * 尝试获取一个中缀表达式Token的解析器
     *
     * @return Token解析器
     */
    const InfixParser *Parser::infixParserFor(const TokenType type) {
        const auto it = S_InfixParsers.find(type);
        return it != S_InfixParsers.end() ? it->second : nullptr;
    }

    /////////////////////////////////////////////////////////////////

    DeclNode *VarDeclParser::parse(Result &r, Parser *parser, Token *token) const {
        VarDeclNode *varDeclNode{ nullptr };
        if(!parser->peek(TokenType::Identifier))
            return nullptr;

        Token identifier;
        const auto line = identifier.location;
        parser->current(identifier);
        parser->consume();

        if(parser->peek(TokenType::SemiColon)) {
            parser->consume();
            varDeclNode = parser->astBuilder()->makeNode<VarDeclNode>(identifier, nullptr, nullptr);
            varDeclNode->location = line;
            return varDeclNode;
        }

        ExprNode *rhs{};
        if(parser->peek(TokenType::Assignment)) {
            parser->consume();
            rhs = parser->parseExpression(r);

            if(!rhs)
                return nullptr;
        }

        VarDeclNode *varDecl{};
        if(parser->peek(TokenType::Comma)) {
            parser->consume();
            varDecl = dynamic_cast<VarDeclNode *>(parse(r, parser, token));
            if(!varDecl)
                return nullptr;
        }

        if(!varDecl && !parser->expect(r, TokenType::SemiColon)) {
            return nullptr;
        }

        varDeclNode = parser->astBuilder()->makeNode<VarDeclNode>(identifier, rhs, varDecl);
        varDeclNode->location = line;
        if(rhs)
            varDeclNode->location.end(rhs->location.end());

        return varDeclNode;
    }

    DeclNode *FunctionDeclParser::parse(Result &r, Parser *parser, Token *token) const {
        Token identifier{};
        if(!parser->consume(identifier))
            return nullptr;

        auto *functionDeclNode = parser->astBuilder()->makeNode<FunctionDeclNode>(identifier);

        // it's ok
        // function a {
        // }
        if(parser->peek(TokenType::LParenthesis)) {
            if(!parseParameters(r, parser, functionDeclNode)) {
                return nullptr;
            }
        }

        if(!parser->peek(TokenType::LeftCurlyBrace)) {
            parser->error(r, "function expect {", token->location);
            return nullptr;
        }

        auto *body = dynamic_cast<BlockStmtNode *>(parser->parseStatement(r));


        if(!body)
            return nullptr;

        functionDeclNode->body = body;
        functionDeclNode->location = body->location;

        return functionDeclNode;
    }

    DeclNode *ClassDeclParser::parse(Result &r, Parser *parser, Token *token) const {
        Token identifier{};
        if(!parser->consume(identifier))
            return nullptr;

        auto *classDeclNode = parser->astBuilder()->makeNode<ClassDeclNode>(identifier);

        if(!parser->peek(TokenType::LeftCurlyBrace)) {
            parser->error(r, "class expect {", token->location);
            return nullptr;
        }

        parser->consume();

        auto *body = parser->astBuilder()->makeNode<BlockStmtNode>();

        while(!parser->peek(TokenType::RightCurlyBrace) && !parser->peek(TokenType::EndOfFile)) {
            auto *stmt = parser->parseDeclaration(r);
            if(!stmt)
                return nullptr;

            if(!dynamic_cast<FunctionDeclNode *>(stmt) && !dynamic_cast<VarDeclNode *>(stmt)) {
                parser->error(r, "class expect function or var", stmt->location);
                return nullptr;
            }

            body->childrens.push_back(stmt);
        }

        if(parser->peek(TokenType::RightCurlyBrace)) {
            parser->consume();
        } else {
            parser->error(r, "class expect }", token->location);
            return nullptr;
        }

        classDeclNode->body = body;
        classDeclNode->location.start(token->location.start());
        classDeclNode->location.end(body->location.end());

        return classDeclNode;
    }


    StmtNode *BlockStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        auto *scope = parser->astBuilder()->makeNode<BlockStmtNode>();

        scope->location.start(token->location.start());

        parser->parseScope(r, scope, TokenType::RightCurlyBrace);

        if(!parser->peek(TokenType::RightCurlyBrace)) {
            parser->error(r, "scope expected token '}'", token->location);

            return nullptr;
        }

        Token terminatorToken{};
        parser->current(terminatorToken);
        parser->consume();

        scope->location.end(terminatorToken.location.end());
        return scope;
    }

    StmtNode *IfStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        const auto *test = createExpressionNode(r, parser);

        if(!test)
            return nullptr;

        auto *body = parser->parseStatement(r);

        if(!body)
            return nullptr;

        auto *bodyScope = dynamic_cast<BlockStmtNode *>(body);

        if(!bodyScope) {
            bodyScope = parser->astBuilder()->makeNode<BlockStmtNode>();
            bodyScope->childrens.push_back(parser->astBuilder()->makeNode<StmtDeclNode>(body));
        }

        auto *ifNode = parser->astBuilder()->makeNode<IfStmtNode>(test, bodyScope);

        ifNode->location.start(token->location.start());
        ifNode->location.end(ifNode->body->location.end());

        if(parser->peek(TokenType::Else)) {
            Token elseToken{};
            parser->current(elseToken);
            parser->consume();

            auto *elseBody = parser->parseStatement(r);

            if(!elseBody)
                return nullptr;

            auto *elseBodyScope = dynamic_cast<BlockStmtNode *>(elseBody);

            if(!elseBodyScope) {
                elseBodyScope = parser->astBuilder()->makeNode<BlockStmtNode>();
                elseBodyScope->childrens.push_back(parser->astBuilder()->makeNode<StmtDeclNode>(elseBody));
            }
            ifNode->elseBody = elseBodyScope;

            ifNode->location.end(ifNode->elseBody->location.end());
        }

        return ifNode;
    }

    StmtNode *SwitchStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        // TODO:
        const auto *test = createExpressionNode(r, parser);

        if(!test)
            return nullptr;

        auto *switchNode = parser->astBuilder()->makeNode<SwitchStmtNode>(test);
        switchNode->location.start(token->location.start());

        auto *scope = parser->astBuilder()->makeNode<BlockStmtNode>();
        scope->location.start(test->location.end());

        if(!parser->peek(TokenType::LeftCurlyBrace)) {
            parser->error(r, "switch expected token '{'", token->location);
            return nullptr;
        }

        parser->consume();

        while(parser->peek(TokenType::Case)) {
            parser->consume();
            auto *node = parser->parseExpression(r);
            if(!node) {
                parser->error(r, "case expected expression", token->location);
                return nullptr;
            }

            switchNode->matchCases.push_back(node);

            if(!parser->peek(TokenType::Colon)) {
                parser->error(r, "case expected ':'", token->location);
                return nullptr;
            }

            Token colonToken{};
            parser->consume(colonToken);

            auto *caseScope = parser->astBuilder()->makeNode<BlockStmtNode>();
            caseScope->location.start(colonToken.location.start());

            while(DeclNode *declNode = parser->parseDeclaration(r)) {
                caseScope->childrens.push_back(declNode);
            }

            if(caseScope->childrens.empty()) {
                switchNode->matchBodies.push_back(nullptr);
                continue;
            }

            caseScope->location.end(caseScope->childrens.back()->location.end());

            switchNode->matchBodies.push_back(caseScope);
        }

        if(parser->peek(TokenType::Default)) {
            if(!parser->peek(TokenType::Colon)) {
                parser->error(r, "switch default branch expected ':'", token->location);
                return nullptr;
            }

            auto *defaultScope = parser->astBuilder()->makeNode<BlockStmtNode>();

            while(DeclNode *declNode = parser->parseDeclaration(r)) {
                defaultScope->childrens.push_back(declNode);
            }
            switchNode->defaultBody = defaultScope;
        }

        if(!parser->expect(r, TokenType::RightCurlyBrace)) {
            return nullptr;
        }

        Token closeToken{};
        parser->consume(closeToken);

        scope->location.end(closeToken.location.end());
        switchNode->location.end(scope->location.end());

        return switchNode;
    }

    StmtNode *DoWhileStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        auto *body = parser->parseStatement(r);
        if(!body)
            return nullptr;

        auto *bodyScope = dynamic_cast<BlockStmtNode *>(body);
        if(!bodyScope) {
            bodyScope = parser->astBuilder()->makeNode<BlockStmtNode>();
            bodyScope->childrens.push_back(parser->astBuilder()->makeNode<StmtDeclNode>(body));
        }

        if(!parser->expect(r, TokenType::While))
            return nullptr;
        const auto *test = createExpressionNode(r, parser);
        if(!test)
            return nullptr;
        if(!parser->expect(r, TokenType::SemiColon))
            return nullptr;

        const auto node = parser->astBuilder()->makeNode<DoWhileStmtNode>(bodyScope, test);
        node->location.start(token->location.start());
        node->location.end(token->location.end());
        return node;
    }

    StmtNode *ForStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        if(!parser->expect(r, TokenType::LParenthesis))
            return nullptr;

        DeclNode *initDecl{ nullptr };
        if(!parser->peek(TokenType::SemiColon)) {
            auto *expr = parser->parseExpression(r);
            if(!expr)
                return nullptr;
            initDecl = parser->astBuilder()->makeNode<StmtDeclNode>(parser->astBuilder()->makeNode<ExprStmtNode>(expr));
        }
        if(!parser->expect(r, TokenType::SemiColon))
            return nullptr;

        ExprNode *condExpr{ nullptr };
        if(!parser->peek(TokenType::SemiColon)) {
            condExpr = parser->parseExpression(r);
            if(!condExpr)
                return nullptr;
        }
        if(!parser->expect(r, TokenType::SemiColon))
            return nullptr;

        ExprNode *stepExpr{ nullptr };
        if(!parser->peek(TokenType::RParenthesis)) {
            stepExpr = parser->parseExpression(r);
            if(!stepExpr)
                return nullptr;
        }
        if(!parser->expect(r, TokenType::RParenthesis))
            return nullptr;

        auto *body = parser->parseStatement(r);
        if(!body)
            return nullptr;

        auto *bodyScope = dynamic_cast<BlockStmtNode *>(body);
        if(!bodyScope) {
            bodyScope = parser->astBuilder()->makeNode<BlockStmtNode>();
            bodyScope->childrens.push_back(parser->astBuilder()->makeNode<StmtDeclNode>(body));
        }

        const auto node = parser->astBuilder()->makeNode<ForStmtNode>(initDecl, condExpr, stepExpr, bodyScope);
        node->location.start(token->location.start());
        node->location.end(token->location.end());
        return node;
    }

    StmtNode *WhileStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        const auto *test = createExpressionNode(r, parser);
        if(!test)
            return nullptr;

        auto *body = parser->parseStatement(r);
        if(!body)
            return nullptr;

        auto *bodyScope = dynamic_cast<BlockStmtNode *>(body);
        if(!bodyScope) {
            bodyScope = parser->astBuilder()->makeNode<BlockStmtNode>();
            bodyScope->childrens.push_back(parser->astBuilder()->makeNode<StmtDeclNode>(body));
        }

        const auto whileNode = parser->astBuilder()->makeNode<WhileStmtNode>(test, bodyScope);

        whileNode->location.start(token->location.start());
        whileNode->location.end(token->location.end());

        return whileNode;
    }

    StmtNode *BreakStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        if(!parser->expect(r, TokenType::SemiColon))
            return nullptr;

        return parser->astBuilder()->makeNode<BreakStmtNode>();
    }

    StmtNode *ContinueStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        if(!parser->expect(r, TokenType::SemiColon))
            return nullptr;

        return parser->astBuilder()->makeNode<ContinueStmtNode>();
    }

    StmtNode *ReturnStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        if(!parser->peek(TokenType::SemiColon)) {
            const auto expr = parser->parseExpression(r);

            if(!parser->expect(r, TokenType::SemiColon))
                return nullptr;
            return parser->astBuilder()->makeNode<ReturnStmtNode>(expr);
        }

        if(!parser->expect(r, TokenType::SemiColon))
            return nullptr;

        return parser->astBuilder()->makeNode<ReturnStmtNode>(nullptr);
    }

    /**
     * 中缀二目运算符解析
     *
     * @param r 结果
     * @param parser root解析器
     * @param lhs 左AstNode
     * @param token 符号
     * @return 一个二目运算AstNode
     */
    ExprNode *BinaryOperatorInfixParser::parse(Result &r, Parser *parser, ExprNode *lhs, Token *token) const {
        const auto associativePrecedence =
            static_cast<Precedence>(static_cast<uint8_t>(_precedence) - (_isRightAssociative ? 1 : 0));

        const auto rhs = parser->parseExpression(r, associativePrecedence);
        if(!rhs) {
            parser->error(r, "binary operator expects right-hand-side expression", token->location);
            return nullptr;
        }

        const auto binOpNode =
            parser->astBuilder()->makeNode<BinaryExprNode>(Token{ AssignToken::strip(token->type()) }, lhs, rhs);

        if(!_withAssignment)
            return binOpNode;

        const IdentifierExprNode *symbolExprNode = dynamic_cast<IdentifierExprNode *>(lhs);

        if(!symbolExprNode) {
            parser->error(r, "assignment operator left-hand-side expects identifier", lhs->location);
            return nullptr;
        }

        if(AssignToken::support(token->type())) {
            return parser->astBuilder()->makeNode<AssignExprNode>(symbolExprNode, binOpNode);
        }

        return parser->astBuilder()->makeNode<AssignExprNode>(symbolExprNode, rhs);
    }

    ExprNode *ProcCallInfixParser::parse(Result &r, Parser *parser, ExprNode *lhs, Token *token) const {
        // check
        if(!dynamic_cast<IdentifierExprNode *>(lhs)) {
            if(const auto binaryExprNode = dynamic_cast<BinaryExprNode *>(lhs);
               !binaryExprNode || *binaryExprNode->token != TokenType::Dot) {
                parser->error(r, "proc call expect identifier", token->location);
                return nullptr;
            }
        }
        const auto procCallExprNode = parser->astBuilder()->makeNode<ProcCallExprNode>(lhs);

        if(!parser->peek(TokenType::RParenthesis)) {
            if(!parseArguments(r, parser, procCallExprNode)) {
                return nullptr;
            }
        }

        if(!parser->expect(r, TokenType::RParenthesis))
            return nullptr;

        return procCallExprNode;
    }


    ExprNode *ConstValPrefixParser::parse(Result &, Parser *parser, Token *token) const {
        return parser->astBuilder()->makeNode<ValueExprNode>(*token);
    }

    ExprNode *UnaryOperatorPrefixParser::parse(Result &r, Parser *parser, Token *token) const {
        const auto *rhs = parser->parseExpression(r, _precedence);
        if(!rhs) {
            parser->error(r, "unary operator expects right-hand-side expression", token->location);
            return nullptr;
        }

        const auto node = parser->astBuilder()->makeNode<UnaryExprNode>(*token, rhs);

        return node;
    }

    ExprNode *IdentifierPrefixParser::parse(Result &r, Parser *parser, Token *token) const {
        return parser->astBuilder()->makeNode<IdentifierExprNode>(*token);
    }

    ExprNode *ParenthesizedPrefixParser::parse(Result &r, Parser *parser, Token *token) const {
        // 解析括号内的表达式
        auto *expr = parser->parseExpression(r);
        if(!expr) {
            parser->error(r, "expected expression inside parentheses", token->location);
            return nullptr;
        }

        // 期望右括号
        if(!parser->expect(r, TokenType::RParenthesis)) {
            return nullptr;
        }

        return expr;
    }
} // namespace cial::Syntax
