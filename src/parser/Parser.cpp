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

#include <cassert>
#include <fmt/format.h>

#include "../ast/AstFormatter.hpp"

#include "../ast/ExprNode.hpp"
#include "../ast/StmtNode.hpp"

namespace Ciallang::Syntax {
    using namespace Common;

    static ExprNode* createExpressionNode(Result& r, Parser* parser) {
        // expect "("
        if(!parser->expect(r, &S_LParenthesis))
            return nullptr;

        const auto node = parser->parseExpression(r);

        // expect ")"
        if(!parser->expect(r, &S_RParenthesis))
            return nullptr;
        return node;
    }

    /*  static void pairsToList(AstNode *target, AstNode *root) {
          if (root == nullptr)
              return;

          if (root->type != AstNodeType::pair) {
              target->location = root->location;
              target->children.push_back(root);
              return;
          }

          auto currentPair = root;
          target->location.start(currentPair->location.start());
          while (true) {
              if (currentPair->lhs->type != AstNodeType::pair) {
                  if (currentPair->rhs != nullptr)
                      target->children.push_back(currentPair->rhs);
                  target->children.push_back(currentPair->lhs);
                  target->location.end(currentPair->lhs->location.end());
                  break;
              }
              target->children.push_back(currentPair->rhs);
              currentPair = currentPair->lhs;
          }

          ranges::reverse(target->children);
      }*/

    /* static StmtNode *createTypeDeclarationNode(
             Result &r, Parser *parser, AstNode *lhs, const Token &token
     ) {
         const auto node = parser->astBuilder()->typeDeclarationNode();
         node->location.start(token.location.start());

         node->lhs = parser->parseExpression(r, Precedence::type);
         node->location.end(node->lhs->location.end());

         if (lhs != nullptr) {
             lhs->rhs = node;
             return lhs;
         }

         return node;
     }*/

    void Parser::writeAstGraph(
        const filesystem::path& path, AstNode* programNode
    ) {
        auto closeRequired = false;
        FILE* astOutputFile = stdout;
        if(!path.empty()) {
            fopen_s(&astOutputFile,
                    path.string().c_str(),
                    "wt");
            closeRequired = true;
        }

        AstFormatter formatter(programNode, astOutputFile);
        formatter.format(fmt::format("AST Graph: {}", path.string()));

        if(closeRequired) fclose(astOutputFile);
    }

    bool Parser::lookAhead(const size_t count) {
        while(count >= tokens().size() && _lexer.hasNext()) {
            Token* token{ nullptr };
            if(_lexer.next(token)) {
                assert(token != nullptr);

                while(token->type() == TokenType::BlockComment
                      || token->type() == TokenType::LineComment) {
                    if(!_lexer.next(token))
                        return !tokens().empty();
                }
            }
        }
        return !tokens().empty();
    }

    bool Parser::peek(const TokenType type) {
        if(!lookAhead(0)) return false;
        const auto& token = tokens().front();
        return token->type() == type;
    }

    bool Parser::consume() {
        Token token{};
        return consume(token);
    }

    bool Parser::consume(Token& token) {
        if(!lookAhead(0))
            return false;

        return _lexer.tackOverToken(token);
    }

    bool Parser::current(Token*& token) {
        // just check tokens is empty? current tokens is empty we lex,
        if(!lookAhead(0))
            return false;

        token = tokens().front();

        return token->type() != TokenType::EndOfFile;
    }

    bool Parser::expect(Result& r, const Token* token) {
        if(!lookAhead(0)) return false;

        std::string expectedName = token->name();
        const auto expectedType = token->type();
        Token tToken{};

        if(!_lexer.tackOverToken(tToken)) {
            error(r,
                  fmt::format(
                              "expected token '{}' but end of file.",
                              expectedName,
                              tToken.name()), tToken.location
                 );
            return false;
        }

        if(tToken.type() != expectedType) {
            error(r,
                  fmt::format(
                              "expected token '{}' but found '{}'.",
                              expectedName,
                              tToken.name()), tToken.location
                 );
            return false;
        }

        return true;
    }

    /**
     * 获取下一个Token优先级
     * @return Token优先级
     */
    Precedence Parser::currentInfixPrecedence() {
        if(!lookAhead(0))
            return Precedence::lowest;

        const auto* token = tokens().front();
        const auto infixParser = infixParserFor(token->type());
        if(infixParser != nullptr)
            return infixParser->precedence();

        return Precedence::lowest;
    }

    void Parser::synchronize() {
        while(!peek(TokenType::EndOfFile)) {
            Token* token{ nullptr };
            if(!current(token)) return;

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
                    if(!consume()) return;
            }
        }
    }

    StmtNode* Parser::parse(Result& r) {
        auto* global = _astBuilder.beginScope();
        parseScope(r, global);
        return _astBuilder.endScope();
    }

    void Parser::parseScope(
        Result& r, BlockStmtNode* blockStmtNode, const TokenType terminatorToken
    ) {
        while(!peek(TokenType::EndOfFile)) {
            if(peek(terminatorToken)) return;
            auto* statement = parseStatement(r);

            // error sync
            if(!statement) {
                synchronize();
                continue;
            }

            blockStmtNode->children.push_back(statement);
        }
    }

    ExprNode* Parser::parseExpression(
        Result& r, const Precedence precedence
    ) {
        Token token{};
        if(!consume(token)) return nullptr;

        // 前缀
        const auto* prefixParser = prefixParserFor(token.type());

        if(prefixParser == nullptr) {
            error(r,
                  fmt::format("unable prefix parse for token '{}' not found parser.", token.name()),
                  token.location);
            return nullptr;
        }

        ExprNode* lhs = prefixParser->parse(r, this, &token);
        if(lhs == nullptr) {
            error(r,
                  "unexpected empty ast node.",
                  token.location);
            return nullptr;
        }

        // 中缀
        while(precedence < currentInfixPrecedence()) {
            if(!consume(token)) {
                break;
            }

            const auto infixParser = infixParserFor(token.type());
            if(infixParser == nullptr) {
                error(
                      r,
                      fmt::format("unable infix parse for token '{}' not found parser.", token.name()),
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
    StmtNode* Parser::parseStatement(Result& r) {
        Token* token = nullptr;
        // just peek
        if(!current(token)) return nullptr;

        const auto stmtParser = stmtParserFor(token->type());
        StmtNode* statementNode = nullptr;

        if(stmtParser != nullptr) {
            Token consumeToken{};
            consume(consumeToken);
            statementNode = stmtParser->parse(r, this, &consumeToken);
        } else {
            // maybe ExpressionStatement
            auto* expr = parseExpression(r);
            if(expr != nullptr) {
                statementNode = _astBuilder.makeExprStmtNode(expr);
                statementNode->location = expr->location;

                // ;
                if(!expect(r, &S_SemiColon)) {
                    return nullptr;
                }
            }
        }

        return statementNode;
    }

    const StmtParser* Parser::stmtParserFor(const TokenType type) {
        const auto it = S_StmtParsers.find(type);
        if(it != S_StmtParsers.end())
            return it->second;
        return nullptr;
    }

    /**
     * 尝试获取一个前缀表达式Token的解析器
     *
     * @param type if(not has TokenParser) -> nullptr
     *             else -> TokenParser
     * @return Token解析器
     */
    const PrefixParser* Parser::prefixParserFor(const TokenType type) {
        const auto it = S_PrefixParsers.find(type);
        if(it != S_PrefixParsers.end())
            return it->second;
        return nullptr;
    }

    /**
     * 尝试获取一个中缀表达式Token的解析器
     *
     * @param type if(has TokenParser) -> nullptr
     *             else -> TokenParser
     * @return Token解析器
     */
    const InfixParser* Parser::infixParserFor(const TokenType type) {
        const auto it = S_InfixParsers.find(type);
        if(it != S_InfixParsers.end())
            return it->second;
        return nullptr;
    }

    /////////////////////////////////////////////////////////////////

    StmtNode* BlockStmtParser::parse(
        Result& r, Parser* parser, Token* token
    ) const {
        auto* scope = parser->astBuilder()->beginScope();

        scope->location.start(token->location.start());

        parser->parseScope(r,
                           scope,
                           TokenType::RightCurlyBrace);

        if(!parser->peek(TokenType::RightCurlyBrace)) {
            parser->error(r,
                          "scope expected token '}'",
                          token->location);

            parser->astBuilder()->endScope();
            return nullptr;
        }

        Token* terminatorToken = nullptr;
        parser->current(terminatorToken);
        parser->consume();

        scope->location.end(terminatorToken->location.end());
        return parser->astBuilder()->endScope();
    }

    StmtNode* IfStmtParser::parse(
        Result& r, Parser* parser, Token* token
    ) const {
        const auto* test = createExpressionNode(r, parser);

        if(test == nullptr) return nullptr;

        const auto* body = parser->parseStatement(r);

        if(body == nullptr) return nullptr;

        auto* ifNode = parser->astBuilder()
                             ->makeIfStmtNode(test, body);

        ifNode->location.start(token->location.start());
        ifNode->location.end(ifNode->body->location.end());

        if(parser->peek(TokenType::Else)) {
            Token* elseToken;
            parser->current(elseToken);
            parser->consume();

            ifNode->elseBody = parser->parseStatement(r);

            if(!ifNode->elseBody) return nullptr;

            ifNode->location.end(ifNode->elseBody->location.end());
        }

        return ifNode;
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
    ExprNode* BinaryOperatorInfixParser::parse(
        Result& r, Parser* parser, ExprNode* lhs, Token* token
    ) const {
        const auto associativePrecedence = static_cast<Precedence>(
            static_cast<uint8_t>(_precedence) - (_isRightAssociative ? 1 : 0)
        );

        const auto rhs = parser->parseExpression(r, associativePrecedence);
        if(rhs == nullptr) {
            parser->error(
                          r,
                          "binary operator expects right-hand-side expression",
                          token->location);
            return nullptr;
        }

        const auto binOpNode = parser
                               ->astBuilder()
                               ->makeBinaryExprNode(stripAssign(std::move(*token)), lhs, rhs);

        if(!_withAssignment) return binOpNode;

        const SymbolExprNode* symbolExprNode = dynamic_cast<SymbolExprNode*>(lhs);

        if(!symbolExprNode) {
            parser->error(r,
                          "assignment operator left-hand-side expects identifier",
                          lhs->location);
            return nullptr;
        }

        if(contains(S_AssignToNonAssign, token->type())) {
            return parser->astBuilder()->makeAssignExprNode(symbolExprNode, binOpNode);
        }

        return parser->astBuilder()->makeAssignExprNode(symbolExprNode, rhs);
    }

    ExprNode* ConstValPrefixParser::parse(Result&, Parser* parser, Token* token) const {
        return parser->astBuilder()->makeValueExprNode(std::move(*token));
    }

    ExprNode* UnaryOperatorPrefixParser::parse(
        Result& r, Parser* parser, Token* token
    ) const {
        const auto rhs = parser->parseExpression(r, _precedence);
        if(rhs == nullptr) {
            parser->error(
                          r,
                          "unary operator expects right-hand-side expression",
                          token->location);
            return nullptr;
        }

        const auto node = parser
                          ->astBuilder()
                          ->makeUnaryExprNode(std::move(*token), rhs);

        return node;
    }

    ExprNode* SymbolPrefixParser::parse(
        Result& r, Parser* parser, Token* token
    ) const {
        return parser->astBuilder()->makeSymbolExprNode(std::move(*token));
    }
}
