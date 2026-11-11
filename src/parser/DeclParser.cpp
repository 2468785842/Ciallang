// Copyright (c) 2026/11/11
//
// Parser 声明模式（doc/declarations.md）：.d.tjs → decl::Module。
// 复用 Lexer 与 Parser 的 token/诊断设施；文档注释须在 lookAhead 的注释
// 过滤前直接驱动词法队列取走（/** */ 原文按 span 截取后规范化）。
// 所有语法诊断码以 decl. 为前缀；词法/预处理 Error token 原样透传。
#include <fmt/format.h>

#include <functional>
#include <utility>
#include <vector>

#include "parser/Parser.hpp"

namespace cial::syntax {
    namespace {
        // 文档注释规范化：去 /** */ 标记、逐行去对齐星号、去首尾空行；
        // 与 DeclModel.cpp 的生成格式互逆（单行 "/** text */"，多行逐行 " * text"）
        std::string normalizeDocComment(const std::string &raw) {
            if(raw.size() < 5 || raw.compare(0, 3, "/**") != 0)
                return {};
            size_t bodyEnd = raw.size();
            if(raw.compare(raw.size() - 2, 2, "*/") == 0)
                bodyEnd -= 2;
            const std::string body = raw.substr(3, bodyEnd - 3);
            const auto isSpace = [](const char ch) { return ch == ' ' || ch == '\t' || ch == '\r'; };
            std::vector<std::string> lines;
            size_t start = 0;
            for(;;) {
                const auto nl = body.find('\n', start);
                std::string line = body.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
                size_t b = 0;
                size_t e = line.size();
                while(b < e && isSpace(line[b]))
                    ++b;
                while(e > b && isSpace(line[e - 1]))
                    --e;
                line = line.substr(b, e - b);
                if(!line.empty() && line[0] == '*') {
                    line = line.substr(1);
                    b = 0;
                    while(b < line.size() && isSpace(line[b]))
                        ++b;
                    line = line.substr(b);
                }
                lines.push_back(std::move(line));
                if(nl == std::string::npos)
                    break;
                start = nl + 1;
            }
            while(!lines.empty() && lines.front().empty())
                lines.erase(lines.begin());
            while(!lines.empty() && lines.back().empty())
                lines.pop_back();
            std::string out;
            for(size_t i = 0; i < lines.size(); ++i) {
                if(i)
                    out += '\n';
                out += lines[i];
            }
            return out;
        }
    } // namespace

    decl::Module Parser::parseDeclarations(Result &r) {
        decl::Module module;

        // 注释排空 + 文档捕获：直接驱动词法器直至非注释 token 入队或 EOF，
        // 最后一个 /** */ 作为紧邻声明的文档（普通 /* */ 与 // 不入文档）
        auto takeDocComment = [&]() -> std::string {
            std::string doc;
            for(;;) {
                if(_lexer.tokenSize() == 0) {
                    if(!_lexer.hasNext())
                        break;
                    Token *next{};
                    if(!_lexer.next(next))
                        break;
                    continue;
                }
                Token *front{};
                _lexer.peekToken(front);
                if(front->type() != TokenType::LineComment && front->type() != TokenType::BlockComment)
                    break;
                std::string text;
                if(front->type() == TokenType::BlockComment && front->span.valid())
                    text = _sourceFile.substring(front->span.begin, front->span.end);
                Token taken{};
                _lexer.takeOverToken(taken);
                if(!text.empty() && text.compare(0, 3, "/**") == 0)
                    doc = normalizeDocComment(text);
            }
            return doc;
        };

        // 声明头名字；失败指向实际 token（EOF 时退回锚点）。decl 模式的语法
        // 诊断统一走 decl.* 码，不用通用 expect
        auto expectIdentifier = [&](Token &out, const Token &anchor, const char *what) -> bool {
            if(!peek(r, TokenType::Identifier)) {
                Token *bad{};
                error(r, fmt::format("{} must be an identifier", what), current(r, bad) ? *bad : anchor,
                      "decl.expected-token");
                return false;
            }
            consume(r, out);
            return true;
        };

        auto expectSemi = [&](const Token &anchor) -> bool {
            if(peek(r, TokenType::SemiColon)) {
                consume(r);
                return true;
            }
            Token *bad{};
            error(r, "expected ';' after declaration", current(r, bad) ? *bad : anchor, "decl.expected-token");
            return false;
        };

        // variable_type（tjs.y:339）：可选 ": 标识符|void|int|real|string|octet"。
        // 关键字规范化为模型名（Integer/Real/String/Octet），与 --api-json 一致。
        // 返回是否出现注解；非法注解已报 decl.invalid-type 并返回 true（out 空）
        auto parseTypeAnnotation = [&](std::string &out) -> bool {
            if(!peek(r, TokenType::Colon))
                return false;
            Token colon{};
            consume(r, colon);
            Token type{};
            if(!consume(r, type) ||
               (type.type() != TokenType::Identifier && type.type() != TokenType::Void &&
                type.type() != TokenType::Int && type.type() != TokenType::Real && type.type() != TokenType::String &&
                type.type() != TokenType::Octet)) {
                error(r, "declaration type expects symbol or void/int/real/string/octet",
                      type.span.valid() ? type : colon, "decl.invalid-type");
                return true;
            }
            switch(type.type()) {
                case TokenType::Void:
                    out = "void";
                    break;
                case TokenType::Int:
                    out = "Integer";
                    break;
                case TokenType::Real:
                    out = "Real";
                    break;
                case TokenType::String:
                    out = "String";
                    break;
                case TokenType::Octet:
                    out = "Octet";
                    break;
                default:
                    out = type.getString().toStdStr();
                    break;
            }
            return true;
        };

        // 默认值仅接受字面量（声明不描述计算）：[负号] 数字/字符串/true/false/null/void；
        // 原文按 span 截取（含引号/负号），供生成器原样回写
        auto parseDefaultLiteral = [&](std::string &out, const Token &paramName) -> bool {
            Token minus{};
            const bool negative = peek(r, TokenType::Minus);
            if(negative)
                consume(r, minus);
            Token *value{};
            if(!current(r, value) ||
               (value->type() != TokenType::ConstVal && value->type() != TokenType::Null &&
                value->type() != TokenType::Void)) {
                Token *bad{};
                error(r, "default value must be a number/string/true/false/null/void literal",
                      current(r, bad) ? *bad : paramName, "decl.invalid-default");
                return false;
            }
            const size_t begin = negative && minus.span.valid() ? minus.span.begin : value->span.begin;
            out = _sourceFile.substring(begin, value->span.end);
            consume(r);
            return true;
        };

        auto parseDeclParams = [&](std::vector<decl::Param> &params, const Token &anchor) -> bool {
            consume(r); // '('
            if(peek(r, TokenType::RParenthesis)) {
                consume(r);
                return true;
            }
            for(;;) {
                if(peek(r, TokenType::Asterisk)) {
                    // 未命名收拢 `*`：变长且必须是最后一项（tjs.y func_decl_arg_collapse）
                    consume(r);
                    decl::Param collapse;
                    collapse.collapse = true;
                    params.push_back(std::move(collapse));
                    if(!peek(r, TokenType::RParenthesis)) {
                        Token *bad{};
                        error(r, "collapse parameter '*' must be the last parameter", current(r, bad) ? *bad : anchor,
                              "decl.expected-token");
                        return false;
                    }
                    consume(r);
                    return true;
                }
                Token name{};
                if(!expectIdentifier(name, anchor, "parameter name"))
                    return false;
                decl::Param param;
                param.name = name.getString().toStdStr();
                std::string type{};
                if(parseTypeAnnotation(type)) {
                    if(type.empty())
                        return false;
                    param.type = std::move(type);
                }
                if(peek(r, TokenType::Assignment)) {
                    consume(r);
                    std::string literal;
                    if(!parseDefaultLiteral(literal, name))
                        return false;
                    param.optional = true;
                    param.defaultText = std::move(literal);
                }
                if(peek(r, TokenType::Asterisk)) {
                    // 命名收拢 `name: type*`（不支持默认值，与 tjs.y 相同）
                    consume(r);
                    if(param.optional) {
                        error(r, "collapse parameter cannot have a default value", name, "decl.invalid-default");
                        return false;
                    }
                    if(!peek(r, TokenType::RParenthesis)) {
                        Token *bad{};
                        error(r, "collapse parameter must be the last parameter", current(r, bad) ? *bad : name,
                              "decl.expected-token");
                        return false;
                    }
                    param.collapse = true;
                    params.push_back(std::move(param));
                    consume(r);
                    return true;
                }
                params.push_back(std::move(param));
                if(peek(r, TokenType::RParenthesis)) {
                    consume(r);
                    return true;
                }
                if(peek(r, TokenType::Comma)) {
                    consume(r);
                    continue;
                }
                Token *bad{};
                error(r, "expected ',' or ')' in parameter list", current(r, bad) ? *bad : name, "decl.expected-token");
                return false;
            }
        };

        // parseOne/parseListFn 相互递归：先声明 std::function 再赋值
        std::function<bool(decl::Symbol *, bool, std::string, Token)> parseOne{};
        std::function<bool(decl::Symbol *, bool, TokenType)> parseListFn{};

        auto parseBracedBody = [&](decl::Symbol &symbol, const bool inClassBody, const char *what,
                                   const Token &anchor) -> bool {
            if(!peek(r, TokenType::LeftCurlyBrace)) {
                Token *bad{};
                error(r, fmt::format("{} expects '{{'", what), current(r, bad) ? *bad : anchor, "decl.expected-token");
                return false;
            }
            consume(r);
            parseListFn(&symbol, inClassBody, TokenType::RightCurlyBrace);
            if(!peek(r, TokenType::RightCurlyBrace)) {
                Token *bad{};
                error(r, fmt::format("{} expects '}}'", what), current(r, bad) ? *bad : anchor, "decl.expected-token");
                return false;
            }
            consume(r);
            return true;
        };

        // 失败项后同步到下一声明关键字/块边界，避免逐 token 级联。
        // 传入所在层的终结符：顶层（EndOfFile）的游离 '}' 不是边界，必须消费掉，
        // 否则与 parseOne 的 not-a-declaration 互相死循环
        auto syncDecl = [&](const TokenType terminator) {
            for(;;) {
                Token *token{};
                if(!current(r, token))
                    return;
                if(token->type() == terminator && terminator != TokenType::EndOfFile)
                    return;
                switch(token->type()) {
                    case TokenType::Function:
                    case TokenType::Class:
                    case TokenType::Var:
                    case TokenType::Const:
                    case TokenType::Property:
                        return;
                    default:
                        if(!consume(r))
                            return;
                }
            }
        };

        parseOne = [&](decl::Symbol *parent, const bool inClassBody, std::string doc, Token kw) -> bool {
            decl::Symbol symbol;
            symbol.doc = std::move(doc);

            // `namespace` 是声明模式的上下文关键字（保留字表不含，脚本模式不变）；
            // 其余普通标识符不是声明
            const bool isNamespace = kw.type() == TokenType::Identifier && kw.getString().toStdStr() == "namespace";
            if(kw.type() == TokenType::Identifier && !isNamespace) {
                error(r, "declaration files accept only namespace/class/function/property/var declarations", kw,
                      "decl.not-a-declaration");
                return false;
            }

            bool isStatic = false;
            if(kw.type() == TokenType::Static) {
                if(!inClassBody) {
                    error(r, "'static' is only allowed on class member functions", kw, "decl.static-outside-class");
                    return false;
                }
                // 只消费 'static'；'function' 留在队首交由 Function 分支统一消费
                consume(r);
                if(!peek(r, TokenType::Function)) {
                    Token *bad{};
                    error(r, "'static' must be followed by 'function'", current(r, bad) ? *bad : kw,
                          "decl.expected-token");
                    return false;
                }
                Token *fn{};
                current(r, fn);
                kw = *fn;
                isStatic = true;
            }

            switch(kw.type()) {
                case TokenType::SemiColon:
                    consume(r);
                    return true;

                case TokenType::Identifier: {
                    // 仅 `namespace`（已判定 isNamespace）
                    consume(r);
                    if(inClassBody) {
                        error(r, "namespace declaration is not allowed inside a class body", kw,
                              "decl.invalid-nesting");
                        return false;
                    }
                    symbol.kind = decl::SymbolKind::Namespace;
                    Token name{};
                    if(!expectIdentifier(name, kw, "namespace name"))
                        return false;
                    symbol.name = name.getString().toStdStr();
                    symbol.span = name.span;
                    if(!parseBracedBody(symbol, false, "namespace", name))
                        return false;
                    (parent ? parent->members : module.symbols).push_back(std::move(symbol));
                    return true;
                }

                case TokenType::Class: {
                    consume(r);
                    symbol.kind = decl::SymbolKind::Class;
                    Token name{};
                    if(!expectIdentifier(name, kw, "class name"))
                        return false;
                    symbol.name = name.getString().toStdStr();
                    symbol.span = name.span;
                    if(peek(r, TokenType::Extends)) {
                        consume(r);
                        for(;;) {
                            Token base{};
                            if(!expectIdentifier(base, name, "base class name"))
                                return false;
                            symbol.extends.push_back(base.getString().toStdStr());
                            if(!peek(r, TokenType::Comma))
                                break;
                            consume(r);
                        }
                    }
                    if(!parseBracedBody(symbol, true, "class", name))
                        return false;
                    break;
                }

                case TokenType::Function: {
                    consume(r);
                    symbol.kind = isStatic ? decl::SymbolKind::StaticFunction : decl::SymbolKind::Function;
                    Token name{};
                    if(!expectIdentifier(name, kw, "function name"))
                        return false;
                    symbol.name = name.getString().toStdStr();
                    symbol.span = name.span;
                    if(peek(r, TokenType::LParenthesis) && !parseDeclParams(symbol.params, name))
                        return false;
                    std::string returnType;
                    if(parseTypeAnnotation(returnType)) {
                        if(returnType.empty())
                            return false;
                        symbol.ret = std::move(returnType);
                    }
                    if(!expectSemi(name))
                        return false;
                    break;
                }

                case TokenType::Property: {
                    consume(r);
                    symbol.kind = decl::SymbolKind::Property;
                    Token name{};
                    if(!expectIdentifier(name, kw, "property name"))
                        return false;
                    symbol.name = name.getString().toStdStr();
                    symbol.span = name.span;
                    if(peek(r, TokenType::Colon)) {
                        // 简写：访问方式未指定（读写性不声称）
                        std::string type;
                        if(parseTypeAnnotation(type)) {
                            if(type.empty())
                                return false;
                            symbol.ret = std::move(type);
                        }
                        if(!expectSemi(name))
                            return false;
                        break;
                    }
                    if(!peek(r, TokenType::LeftCurlyBrace)) {
                        Token *bad{};
                        error(r, "property expects '{' or ':'", current(r, bad) ? *bad : name, "decl.expected-token");
                        return false;
                    }
                    consume(r);
                    for(;;) {
                        Token *handler{};
                        if(!current(r, handler)) {
                            error(r, "property body expects getter or setter", name, "decl.expected-token");
                            return false;
                        }
                        if(handler->type() == TokenType::RightCurlyBrace) {
                            consume(r);
                            break;
                        }
                        if(handler->type() == TokenType::Setter) {
                            Token setter{};
                            consume(r, setter);
                            if(symbol.hasSetter) {
                                error(r, "setter already defined", setter, "decl.duplicate-handler");
                                return false;
                            }
                            if(!peek(r, TokenType::LParenthesis)) {
                                error(r, "setter expects '('", setter, "decl.expected-token");
                                return false;
                            }
                            consume(r);
                            Token paramName{};
                            if(!expectIdentifier(paramName, setter, "setter parameter"))
                                return false;
                            std::string type;
                            if(parseTypeAnnotation(type)) {
                                if(type.empty())
                                    return false;
                                if(symbol.ret.empty())
                                    symbol.ret = std::move(type);
                            }
                            if(!peek(r, TokenType::RParenthesis)) {
                                error(r, "setter expects ')'", paramName, "decl.expected-token");
                                return false;
                            }
                            consume(r);
                            if(!expectSemi(setter))
                                return false;
                            symbol.hasSetter = true;
                            continue;
                        }
                        if(handler->type() == TokenType::Getter) {
                            Token getter{};
                            consume(r, getter);
                            if(symbol.hasGetter) {
                                error(r, "getter already defined", getter, "decl.duplicate-handler");
                                return false;
                            }
                            if(peek(r, TokenType::LParenthesis)) {
                                consume(r);
                                if(!peek(r, TokenType::RParenthesis)) {
                                    error(r, "getter expects ')'", getter, "decl.expected-token");
                                    return false;
                                }
                                consume(r);
                            }
                            std::string type;
                            if(parseTypeAnnotation(type)) {
                                if(type.empty())
                                    return false;
                                symbol.ret = std::move(type);
                            }
                            if(!expectSemi(getter))
                                return false;
                            symbol.hasGetter = true;
                            continue;
                        }
                        error(r, "property body expects getter or setter", *handler, "decl.expected-token");
                        return false;
                    }
                    break;
                }

                case TokenType::Var:
                case TokenType::Const: {
                    consume(r);
                    symbol.kind = decl::SymbolKind::Variable;
                    symbol.constant = kw.type() == TokenType::Const;
                    Token name{};
                    if(!expectIdentifier(name, kw, symbol.constant ? "const name" : "var name"))
                        return false;
                    symbol.name = name.getString().toStdStr();
                    symbol.span = name.span;
                    std::string type;
                    if(parseTypeAnnotation(type)) {
                        if(type.empty())
                            return false;
                        symbol.ret = std::move(type);
                    }
                    if(peek(r, TokenType::Assignment)) {
                        Token assign{};
                        consume(r, assign);
                        error(r, "declaration files do not declare initial values", assign, "decl.initializer");
                        return false;
                    }
                    if(!expectSemi(name))
                        return false;
                    break;
                }

                default:
                    // 词法/预处理 Error token 原样透传（与 parseExpression 同口径），
                    // 其余一律按"不是声明"诊断
                    if(kw.type() == TokenType::Error || kw.type() == TokenType::Invalid)
                        error(r, kw.getString().toStdStr(), kw, kw.code.empty() ? "lex.error" : kw.code);
                    else
                        error(r, "declaration files accept only namespace/class/function/property/var declarations", kw,
                              "decl.not-a-declaration");
                    return false;
            }

            (parent ? parent->members : module.symbols).push_back(std::move(symbol));
            return true;
        };

        parseListFn = [&](decl::Symbol *parent, const bool inClassBody, const TokenType terminator) -> bool {
            for(;;) {
                auto doc = takeDocComment();
                Token *front{};
                if(!current(r, front))
                    break;
                if(front->type() == terminator)
                    break;
                // 块体内 EOF：交给调用方报缺失 '}'，不重复报"不是声明"
                if(terminator != TokenType::EndOfFile && front->type() == TokenType::EndOfFile)
                    break;
                if(!parseOne(parent, inClassBody, std::move(doc), *front))
                    syncDecl(terminator);
            }
            return true;
        };

        parseListFn(nullptr, false, TokenType::EndOfFile);
        return module;
    }
} // namespace cial::syntax

namespace cial::decl {
    Module parseDeclarations(Common::SourceFile &source, Common::Result &diagnostics, PreProcessor &pp) {
        syntax::Parser parser{ source, pp };
        return parser.parseDeclarations(diagnostics);
    }
} // namespace cial::decl
