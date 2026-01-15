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

#include "parser/ast/DeclNode.hpp"
#include "parser/ast/ExprNode.hpp"
#include "parser/ast/StmtNode.hpp"

namespace cial::Syntax {
    using namespace Common;

    static ExprNode *createExpressionNode(Result &r, Parser *parser) {
        // expect "("
        if(!parser->expect(r, TokenType::LParenthesis))
            return nullptr;

        const auto node = parser->parseExpression(r, true);

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

        while(!parser->peek(r, TokenType::RParenthesis)) {
            if(expectArgument) {
                // 检查当前token是否为逗号
                if(parser->peek(r, TokenType::Comma)) {
                    // 遇到逗号，添加隐式void参数
                    node->arguments.push_back(parser->astBuilder()->makeNode<ValueExprNode>(Token{}));

                    // 期望并消耗逗号
                    if(!parser->expect(r, TokenType::Comma))
                        return false;
                    // 继续期望参数
                    expectArgument = true;
                } else {
                    // 解析表达式参数
                    auto *expr = parser->parseExpression(r, false);
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

    static bool parseParameters(Result &r, Parser *parser, Parameters &parameters, const SourceLocation &location) {
        if(!parser->expect(r, TokenType::LParenthesis))
            return false;

        if(parser->peek(r, TokenType::RParenthesis)) {
            parser->consume(r);
            return true;
        }

        for(;;) {
            if(!parser->peek(r, TokenType::Identifier)) {
                parser->error(r, "function parameter expect a identifier", location);
                return false;
            }
            Token identifier{};
            parser->consume(r, identifier);

            // default value
            ExprNode *expr = nullptr;
            if(parser->peek(r, TokenType::Assignment)) {
                parser->consume(r);
                Token *assignmentToken{};
                parser->current(r, assignmentToken);
                expr = parser->parseExpression(r, false);
                if(!expr)
                    return false;
            }

            parameters.emplace_back(std::move(identifier), expr);

            if(parser->peek(r, TokenType::RParenthesis)) {
                parser->consume(r);
                break;
            }

            if(!parser->expect(r, TokenType::Comma))
                return false;
        }

        return true;
    }

    bool Parser::lookAhead(Result &r, const size_t count) {
        while(count > _lexer.tokenSize() && _lexer.hasNext()) {
            Token *token{ nullptr };
            if(!_lexer.next(r, token))
                break;

            if(token->type() == TokenType::LineComment || token->type() == TokenType::BlockComment) {
                _lexer.takeOverToken(*token);
            }
        }
        return _lexer.tokenSize() != 0;
    }

    bool Parser::peek(Result &r, const TokenType tokenType) {
        if(!lookAhead(r, 1))
            return false;
        Token *token;
        _lexer.peekToken(token);
        return token->type() == tokenType;
    }

    bool Parser::consume(Result &r) {
        Token token{};
        return consume(r, token);
    }

    bool Parser::consume(Result &r, Token &token) {
        if(!lookAhead(r, 1))
            return false;

        return _lexer.takeOverToken(token);
    }

    bool Parser::current(Result &r, Token *&token) {
        if(!lookAhead(r, 1))
            return false;

        _lexer.peekToken(token);

        return true;
    }

    bool Parser::expect(Result &r, const TokenType tokenType) {
        if(!lookAhead(r, 1))
            return false;

        std::string expectedName = tokenTypeToStr(tokenType);
        Token tToken{};

        if(!_lexer.takeOverToken(tToken)) {
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
    Precedence Parser::nextInfixPrecedence(Result &r, const bool enableCommaExpr) {
        if(lookAhead(r, 1)) {
            Token *token;
            _lexer.peekToken(token);
            if(const auto infixParser = infixParserFor(token->type(), enableCommaExpr))
                return infixParser->precedence();
        }
        return Precedence::lowest;
    }

    void Parser::synchronize(Result &r) {
        while(lookAhead(r, 1)) {
            Token *token{};
            if(!current(r, token))
                return;

            switch(token->type()) {
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
                    if(!consume(r))
                        return;
            }
        }
    }

    AstNode *Parser::parse(Result &r) {
        auto *global = _astBuilder.makeNode<BlockStmtNode>();
        parseScope(r, global);
        return global;
    }

    bool Parser::parseScope(Result &r, BlockStmtNode *blockStmtNode, const TokenType terminatorToken) {
        bool success = true;
        while(_lexer.hasNext()) {
            if(peek(r, terminatorToken))
                break;
            auto *statement = parseDeclaration(r);

            // error sync
            if(!statement) {
                success = false;
                consume(r);
                synchronize(r);
                continue;
            }

            blockStmtNode->childrens.push_back(statement);
        }
        return success;
    }

    DeclNode *Parser::parseDeclaration(Result &r) {
        Token *token{};
        if(!current(r, token))
            return nullptr;

        if(const auto *declParser = declParserFor(token->type())) {
            Token t{};
            consume(r, t);
            return declParser->parse(r, this, &t);
        }

        if(const auto *stmt = parseStatement(r, true)) {
            return _astBuilder.makeNode<StmtDeclNode>(stmt);
        }

        return nullptr;
    }

    ExprNode *Parser::parseExpression(Result &r, const bool enableCommaExpr, const Precedence pre) {
        Token token{};
        if(!consume(r, token))
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
        while(pre < nextInfixPrecedence(r, enableCommaExpr)) {
            if(!consume(r, token))
                break;

            const auto infixParser = infixParserFor(token.type(), enableCommaExpr);
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
    StmtNode *Parser::parseStatement(Result &r, const bool enableCommaExpr) {
        Token *token{};

        // just peek
        if(!current(r, token))
            return nullptr;

        if(const auto stmtParser = stmtParserFor(token->type())) {
            Token t{};
            consume(r, t);
            return stmtParser->parse(r, this, token);
        }

        // maybe ExpressionStatement

        if(const auto *expr = parseExpression(r, enableCommaExpr)) {
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
    const InfixParser *Parser::infixParserFor(const TokenType type, const bool enableCommaExpr) {
        if(!enableCommaExpr && type == TokenType::Comma)
            return nullptr;
        const auto it = S_InfixParsers.find(type);
        return it != S_InfixParsers.end() ? it->second : nullptr;
    }

    /////////////////////////////////////////////////////////////////

    DeclNode *PropertyDeclParser::parse(Result &r, Parser *parser, Token *token) const {
        Token identifier{};
        if(!parser->consume(r, identifier))
            return nullptr;

        auto *propertyDeclNode = parser->astBuilder()->makeNode<PropertyDeclNode>(identifier);

        if(!parser->peek(r, TokenType::LeftCurlyBrace)) {
            parser->error(r, "property expect {", token->location);
            return nullptr;
        }
        parser->consume(r);

        FunctionDeclNode *setter{ nullptr };
        FunctionDeclNode *getter{ nullptr };

        while(!parser->peek(r, TokenType::RightCurlyBrace)) {

            if(parser->peek(r, TokenType::Setter)) {
                if(setter) {
                    parser->error(r, "setter already defined", setter->location);
                    return nullptr;
                }

                Token setterToken;
                parser->consume(r, setterToken);

                Parameters params;
                if(!parseParameters(r, parser, params, setterToken.location)) {
                    return nullptr;
                }

                if(params.empty()) {
                    parser->error(r, "setter params too few", setterToken.location);
                    return nullptr;
                }

                if(params.size() > 1) {
                    parser->error(r, "setter params too many", setterToken.location);
                    return nullptr;
                }

                if(!parser->peek(r, TokenType::LeftCurlyBrace)) {
                    parser->error(r, "setter expect token '{'", setterToken.location);
                    return nullptr;
                }
                parser->consume(r);

                setter = parser->astBuilder()->makeNode<FunctionDeclNode>(setterToken);
                setter->parameters = std::move(params);
                setter->body = parser->astBuilder()->makeNode<BlockStmtNode>();

                if(!parser->parseScope(r, setter->body, TokenType::RightCurlyBrace)) {
                    return nullptr;
                }

                if(!parser->peek(r, TokenType::RightCurlyBrace)) {
                    parser->error(r, "setter expected token '}'", token->location);
                    return nullptr;
                }
                parser->consume(r);
                continue;
            }

            if(parser->peek(r, TokenType::Getter)) {
                if(getter) {
                    parser->error(r, "getter already defined", getter->location);
                    return nullptr;
                }
                Token getterToken;
                parser->consume(r, getterToken);

                if(parser->peek(r, TokenType::LParenthesis)) {
                    parser->consume(r);
                    if(!parser->expect(r, TokenType::RParenthesis))
                        return nullptr;
                }

                if(!parser->peek(r, TokenType::LeftCurlyBrace)) {
                    parser->error(r, "getter expect token '{'", getterToken.location);
                    return nullptr;
                }

                parser->consume(r);

                getter = parser->astBuilder()->makeNode<FunctionDeclNode>(getterToken);
                getter->body = parser->astBuilder()->makeNode<BlockStmtNode>();

                if(!parser->parseScope(r, getter->body, TokenType::RightCurlyBrace)) {
                    return nullptr;
                }

                if(!parser->peek(r, TokenType::RightCurlyBrace)) {
                    parser->error(r, "getter expected token '}'", token->location);
                    return nullptr;
                }
                parser->consume(r);

                continue;
            }

            Token *t{};
            parser->current(r, t);
            parser->error(r, "expect getter or setter", t->location);
            return nullptr;
        }

        Token end;
        if(!parser->consume(r, end) || end != TokenType::RightCurlyBrace) {
            parser->error(r, "property expect }", token->location);
            return nullptr;
        }

        propertyDeclNode->setter = setter;
        propertyDeclNode->getter = getter;
        propertyDeclNode->location.start(token->location.start());
        propertyDeclNode->location.end(end.location.end());

        return propertyDeclNode;
    }

    DeclNode *VarDeclParser::parse(Result &r, Parser *parser, Token *token) const {
        VarDeclNode *varDeclNode{ nullptr };
        if(!parser->peek(r, TokenType::Identifier))
            return nullptr;

        Token identifier;
        parser->consume(r, identifier);
        const auto line = identifier.location;

        if(parser->peek(r, TokenType::SemiColon)) {
            parser->consume(r);
            varDeclNode = parser->astBuilder()->makeNode<VarDeclNode>(identifier, nullptr, nullptr);
            varDeclNode->location = line;
            return varDeclNode;
        }

        ExprNode *rhs{};
        if(parser->peek(r, TokenType::Assignment)) {
            parser->consume(r);
            rhs = parser->parseExpression(r, false);

            if(!rhs)
                return nullptr;
        }

        VarDeclNode *varDecl{};
        if(parser->peek(r, TokenType::Comma)) {
            parser->consume(r);
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
        if(!parser->consume(r, identifier))
            return nullptr;

        auto *functionDeclNode = parser->astBuilder()->makeNode<FunctionDeclNode>(identifier);

        // it's ok
        // function a {
        // }
        if(parser->peek(r, TokenType::LParenthesis)) {
            if(!parseParameters(r, parser, functionDeclNode->parameters, token->location)) {
                return nullptr;
            }
        }

        if(!parser->peek(r, TokenType::LeftCurlyBrace)) {
            parser->error(r, "function expect {", token->location);
            return nullptr;
        }

        auto *body = dynamic_cast<BlockStmtNode *>(parser->parseStatement(r, true));

        if(!body)
            return nullptr;

        functionDeclNode->body = body;
        functionDeclNode->location = body->location;

        return functionDeclNode;
    }

    DeclNode *ClassDeclParser::parse(Result &r, Parser *parser, Token *token) const {
        Token identifier{};
        if(!parser->consume(r, identifier))
            return nullptr;

        auto *classDeclNode = parser->astBuilder()->makeNode<ClassDeclNode>(identifier);

        Vec<IdentifierExprNode *> extends{};
        if(parser->peek(r, TokenType::Extends)) {
            do {
                parser->consume(r);
                auto *exprNode = dynamic_cast<IdentifierExprNode *>(parser->parseExpression(r, false));
                if(!exprNode) {
                    parser->error(r, "class extends must be identifier", identifier.location);
                    return nullptr;
                }
                extends.push_back(exprNode);
            } while(parser->peek(r, TokenType::Comma));

            classDeclNode->extends = std::move(extends);
        }

        if(!parser->peek(r, TokenType::LeftCurlyBrace)) {
            parser->error(r, "class expect {", token->location);
            return nullptr;
        }

        parser->consume(r);

        FunctionDeclNode *constructor{};
        Vec<VarDeclNode *> varDeclVec{};
        Vec<FunctionDeclNode *> funcDeclVec{};
        Vec<PropertyDeclNode *> propertyDeclVec{};

        while(!parser->peek(r, TokenType::RightCurlyBrace)) {
            auto *stmt = parser->parseDeclaration(r);
            if(!stmt)
                return nullptr;

            if(auto *varDecl = dynamic_cast<VarDeclNode *>(stmt)) {
                varDeclVec.push_back(varDecl);
                continue;
            }

            if(auto *propertyDecl = dynamic_cast<PropertyDeclNode *>(stmt)) {
                propertyDeclVec.push_back(propertyDecl);
                continue;
            }

            if(auto *funcDecl = dynamic_cast<FunctionDeclNode *>(stmt)) {
                if(funcDecl->token.getString() == identifier.getString()) {
                    if(constructor) {
                        parser->error(r, "class already have constructor function", funcDecl->location);
                        return nullptr;
                    }
                    constructor = funcDecl;
                } else {
                    funcDeclVec.push_back(funcDecl);
                }
                continue;
            }

            parser->error(r, "class member expect function or var", stmt->location);
            return nullptr;
        }

        Token end;
        if(parser->peek(r, TokenType::RightCurlyBrace)) {
            parser->consume(r, end);
        } else {
            parser->error(r, "class expect }", token->location);
            return nullptr;
        }

        classDeclNode->constructor = constructor;
        classDeclNode->propertyDeclVec = std::move(propertyDeclVec);
        classDeclNode->varDeclVec = std::move(varDeclVec);
        classDeclNode->funcDeclVec = std::move(funcDeclVec);

        classDeclNode->location.start(token->location.start());
        classDeclNode->location.end(end.location.end());

        return classDeclNode;
    }


    StmtNode *BlockStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        auto *scope = parser->astBuilder()->makeNode<BlockStmtNode>();

        scope->location.start(token->location.start());

        if(!parser->parseScope(r, scope, TokenType::RightCurlyBrace)) {
            return nullptr;
        }

        if(!parser->peek(r, TokenType::RightCurlyBrace)) {
            parser->error(r, "scope expected token '}'", token->location);

            return nullptr;
        }

        Token terminatorToken{};
        parser->consume(r, terminatorToken);

        scope->location.end(terminatorToken.location.end());
        return scope;
    }

    StmtNode *IfStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        const auto *test = createExpressionNode(r, parser);

        if(!test)
            return nullptr;

        auto *body = parser->parseStatement(r, true);

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

        if(parser->peek(r, TokenType::Else)) {
            Token elseToken{};
            parser->consume(r, elseToken);

            auto *elseBody = parser->parseStatement(r, true);

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
        const auto *test = createExpressionNode(r, parser);

        if(!test)
            return nullptr;

        auto *switchNode = parser->astBuilder()->makeNode<SwitchStmtNode>(test);
        switchNode->location.start(token->location.start());

        auto *scope = parser->astBuilder()->makeNode<BlockStmtNode>();
        scope->location.start(test->location.end());

        if(!parser->peek(r, TokenType::LeftCurlyBrace)) {
            parser->error(r, "switch expected token '{'", token->location);
            return nullptr;
        }

        parser->consume(r);

        Vec<ExprNode *> matchCases;

        while(parser->peek(r, TokenType::Case)) {
            parser->consume(r);
            auto *node = parser->parseExpression(r, true);
            if(!node) {
                parser->error(r, "case expected expression", token->location);
                return nullptr;
            }

            matchCases.push_back(node);

            if(!parser->peek(r, TokenType::Colon)) {
                parser->error(r, "case expected ':'", token->location);
                return nullptr;
            }

            Token colonToken{};
            parser->consume(r, colonToken);

            auto *caseScope = parser->astBuilder()->makeNode<BlockStmtNode>();
            caseScope->location.start(colonToken.location.start());

            while(!parser->peek(r, TokenType::Case) && !parser->peek(r, TokenType::Default) &&
                  !parser->peek(r, TokenType::RightCurlyBrace)) {
                DeclNode *declNode = parser->parseDeclaration(r);
                if(!declNode)
                    return nullptr;
                caseScope->childrens.push_back(declNode);
            }

            if(caseScope->childrens.empty()) {
                continue;
            }

            caseScope->location.end(caseScope->childrens.back()->location.end());

            switchNode->matches.emplace_back(std::move(matchCases), caseScope);
            matchCases = {};
        }

        if(parser->peek(r, TokenType::Default)) {
            Token defaultToken{};
            parser->consume(r, defaultToken);

            if(!parser->peek(r, TokenType::Colon)) {
                parser->error(r, "switch default branch expected ':'", defaultToken.location);
                return nullptr;
            }
            parser->consume(r);

            auto *defaultScope = parser->astBuilder()->makeNode<BlockStmtNode>();

            while(!parser->peek(r, TokenType::RightCurlyBrace)) {
                DeclNode *declNode = parser->parseDeclaration(r);
                if(!declNode)
                    return nullptr;
                defaultScope->childrens.push_back(declNode);
            }
            switchNode->defaultBody = defaultScope;
        }

        Token closeToken{};
        parser->consume(r, closeToken);

        if(closeToken != TokenType::RightCurlyBrace) {
            return nullptr;
        }

        scope->location.end(closeToken.location.end());
        switchNode->location.end(scope->location.end());

        return switchNode;
    }

    StmtNode *DoWhileStmtParser::parse(Result &r, Parser *parser, Token *token) const {
        auto *body = parser->parseStatement(r, true);
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
        if(!parser->peek(r, TokenType::SemiColon)) {
            auto *expr = parser->parseExpression(r, true);
            if(!expr)
                return nullptr;
            initDecl = parser->astBuilder()->makeNode<StmtDeclNode>(parser->astBuilder()->makeNode<ExprStmtNode>(expr));
        }
        if(!parser->expect(r, TokenType::SemiColon))
            return nullptr;

        ExprNode *condExpr{ nullptr };
        if(!parser->peek(r, TokenType::SemiColon)) {
            condExpr = parser->parseExpression(r, true);
            if(!condExpr)
                return nullptr;
        }
        if(!parser->expect(r, TokenType::SemiColon))
            return nullptr;

        ExprNode *stepExpr{ nullptr };
        if(!parser->peek(r, TokenType::RParenthesis)) {
            stepExpr = parser->parseExpression(r, true);
            if(!stepExpr)
                return nullptr;
        }
        if(!parser->expect(r, TokenType::RParenthesis))
            return nullptr;

        auto *body = parser->parseStatement(r, true);
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

        auto *body = parser->parseStatement(r, true);
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
        if(!parser->peek(r, TokenType::SemiColon)) {
            const auto expr = parser->parseExpression(r, true);

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

        const auto rhs = parser->parseExpression(r, true, associativePrecedence);
        if(!rhs) {
            parser->error(r, "binary operator expects right-hand-side expression", token->location);
            return nullptr;
        }

        const auto binOpNode =
            parser->astBuilder()->makeNode<BinaryExprNode>(Token{ AssignToken::strip(token->type()) }, lhs, rhs);

        if(!_withAssignment)
            return binOpNode;

        if(AssignToken::support(token->type())) {
            return parser->astBuilder()->makeNode<AssignExprNode>(lhs, binOpNode);
        }

        return parser->astBuilder()->makeNode<AssignExprNode>(lhs, rhs);
    }

    ExprNode *ProcCallInfixParser::parse(Result &r, Parser *parser, ExprNode *lhs, Token *token) const {
        // check
        if(!dynamic_cast<IdentifierExprNode *>(lhs) && !dynamic_cast<FunctionExprNode *>(lhs)) {
            if(const auto binaryExprNode = dynamic_cast<BinaryExprNode *>(lhs);
               !binaryExprNode || binaryExprNode->token != TokenType::Dot) {
                parser->error(r, "proc call expect identifier or function expression", token->location);
                return nullptr;
            }
        }

        const auto procCallExprNode = parser->astBuilder()->makeNode<ProcCallExprNode>(lhs);

        if(!parser->peek(r, TokenType::RParenthesis)) {
            if(!parseArguments(r, parser, procCallExprNode)) {
                return nullptr;
            }
        }

        if(!parser->expect(r, TokenType::RParenthesis))
            return nullptr;

        return procCallExprNode;
    }

    ExprNode *TernaryInfixParser::parse(Result &r, Parser *parser, ExprNode *lhs, Token *token) const {

        const auto expr1 = parser->parseExpression(r, true);
        if(!expr1) {
            parser->error(r, "expected expression left", token->location);
            return nullptr;
        }

        if(!parser->expect(r, TokenType::Colon)) {
            return nullptr;
        }

        const auto expr2 = parser->parseExpression(r, true);

        if(!expr2) {
            parser->error(r, "expected expression right", token->location);
            return nullptr;
        }

        const auto ternaryExpr = parser->astBuilder()->makeNode<TernaryExprNode>(lhs, expr1, expr2);
        ternaryExpr->location = token->location;
        ternaryExpr->location.end(expr2->location.end());
        return ternaryExpr;
    }

    ExprNode *ConstValPrefixParser::parse(Result &, Parser *parser, Token *token) const {
        return parser->astBuilder()->makeNode<ValueExprNode>(*token);
    }

    ExprNode *UnaryOperatorPrefixParser::parse(Result &r, Parser *parser, Token *token) const {
        const auto *rhs = parser->parseExpression(r, true, _precedence);
        if(!rhs) {
            parser->error(r, "unary operator expects right-hand-side expression", token->location);
            return nullptr;
        }

        const auto node = parser->astBuilder()->makeNode<PrefixUnaryExprNode>(*token, rhs);

        return node;
    }

    ExprNode *IdentifierPrefixParser::parse(Result &r, Parser *parser, Token *token) const {
        return parser->astBuilder()->makeNode<IdentifierExprNode>(*token);
    }

    ExprNode *InternalIdentifierPrefixParser::parse(Result &r, Parser *parser, Token *token) const {
        return parser->astBuilder()->makeNode<InternalIdentifierExprNode>(*token);
    }

    ExprNode *ParenthesizedPrefixParser::parse(Result &r, Parser *parser, Token *token) const {
        // 解析括号内的表达式
        auto *expr = parser->parseExpression(r, true);
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

    ExprNode *FunctionPrefixParser::parse(Result &r, Parser *parser, Token *token) const {
        auto *functionExprNode = parser->astBuilder()->makeNode<FunctionExprNode>();

        // it's ok
        // function {
        // }
        if(parser->peek(r, TokenType::LParenthesis)) {
            if(!parseParameters(r, parser, functionExprNode->parameters, token->location)) {
                return nullptr;
            }
        }

        if(!parser->peek(r, TokenType::LeftCurlyBrace)) {
            parser->error(r, "function expect {", token->location);
            return nullptr;
        }

        auto *body = dynamic_cast<BlockStmtNode *>(parser->parseStatement(r, true));

        if(!body)
            return nullptr;

        functionExprNode->body = body;
        functionExprNode->location = body->location;

        return functionExprNode;
    }

    ExprNode *UnaryInfixParser::parse(Result &r, Parser *parser, ExprNode *lhs, Token *token) const {
        // check
        if(!dynamic_cast<IdentifierExprNode *>(lhs)) {
            if(const auto binaryExprNode = dynamic_cast<BinaryExprNode *>(lhs);
               !binaryExprNode || binaryExprNode->token != TokenType::Dot) {
                parser->error(r, "suffix unary expect identifier", token->location);
                return nullptr;
            }
        }

        auto *suffixUnaryExprNode = parser->astBuilder()->makeNode<SuffixUnaryExprNode>(*token, lhs);
        return suffixUnaryExprNode;
    }


} // namespace cial::Syntax
