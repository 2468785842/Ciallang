/*
 * Copyright (c) 2026/1/14.
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

#include "PreProcessor.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "types/String.hpp"

namespace {
    enum class TokenKind {
        End,

        Number,
        Symbol,

        // operators
        Comma,
        Assign,

        OrOr,
        AndAnd,

        BitOr,
        BitXor,
        BitAnd,

        Equal,
        NotEqual,

        LT,
        GT,
        LE,
        GE,

        Plus,
        Minus,
        Mul,
        Div,
        Mod,

        Not,

        LParen,
        RParen,
    };

    struct Token {
        TokenKind kind;
        int value = 0;
        cial::String text{};
    };

    struct ExprValue {
        int value = 0;

        // 是否是可赋值的左值
        bool isLValue = false;

        // 左值变量名（仅当 isLValue=true）
        cial::String name;

        static ExprValue r(const int v) { return { v, false, cial::String{} }; }

        static ExprValue l(const cial::String &n, const int v) { return { v, true, n }; }
    };

    class ExprLexer {
    public:
        explicit ExprLexer(const cial::String &s) : src(s) {}

        Token next() {
            skipSpace();

            if(pos >= src.length())
                return { TokenKind::End };

            const char c = src[pos];
            // number (decimal / hex / binary)
            if(std::isdigit(c)) {

                // hex or binary
                if(c == '0' && pos + 1 < src.length()) {
                    const char n = src[pos + 1];

                    // ---------- hexadecimal ----------
                    if(n == 'x' || n == 'X') {
                        pos += 2; // skip 0x
                        int v = 0;
                        bool hasDigit = false;

                        while(pos < src.length() && std::isxdigit(src[pos])) {
                            hasDigit = true;
                            const char d = src[pos++];
                            v = v * 16 + (std::isdigit(d) ? d - '0' : std::tolower(d) - 'a' + 10);
                        }

                        if(!hasDigit)
                            throw std::runtime_error("invalid hex literal");

                        return { TokenKind::Number, v };
                    }

                    // ---------- binary ----------
                    if(n == 'b' || n == 'B') {
                        pos += 2; // skip 0b
                        int v = 0;
                        bool hasDigit = false;

                        while(pos < src.length() && (src[pos] == '0' || src[pos] == '1')) {
                            hasDigit = true;
                            v = (v << 1) | (src[pos++] - '0');
                        }

                        if(!hasDigit)
                            throw std::runtime_error("invalid binary literal");

                        return { TokenKind::Number, v };
                    }
                }

                // ---------- decimal ----------
                int v = 0;
                while(pos < src.length() && std::isdigit(src[pos])) {
                    v = v * 10 + (src[pos++] - '0');
                }
                return { TokenKind::Number, v };
            }

            // symbol
            if(std::isalpha(c) || c == '_') {
                std::stringstream t{ "" };
                while(pos < src.length() && (std::isalnum(src[pos]) || src[pos] == '_'))
                    t << src[pos++];
                return { TokenKind::Symbol, 0, cial::String{ t.str() } };
            }

            pos++;

            // two-char operators
            auto peek = [&](const char expect) {
                if(pos < src.length() && src[pos] == expect) {
                    pos++;
                    return true;
                }
                return false;
            };

            switch(c) {
                case ',':
                    return { TokenKind::Comma };
                case '=':
                    return peek('=') ? Token{ TokenKind::Equal } : Token{ TokenKind::Assign };

                case '!':
                    return peek('=') ? Token{ TokenKind::NotEqual } : Token{ TokenKind::Not };

                case '|':
                    return peek('|') ? Token{ TokenKind::OrOr } : Token{ TokenKind::BitOr };

                case '&':
                    return peek('&') ? Token{ TokenKind::AndAnd } : Token{ TokenKind::BitAnd };

                case '<':
                    return peek('=') ? Token{ TokenKind::LE } : Token{ TokenKind::LT };
                case '>':
                    return peek('=') ? Token{ TokenKind::GE } : Token{ TokenKind::GT };
                case '^':
                    return { TokenKind::BitXor };
                case '+':
                    return { TokenKind::Plus };
                case '-':
                    return { TokenKind::Minus };
                case '*':
                    return { TokenKind::Mul };
                case '/':
                    return { TokenKind::Div };
                case '%':
                    return { TokenKind::Mod };
                case '(':
                    return { TokenKind::LParen };
                case ')':
                    return { TokenKind::RParen };
                default:;
            }

            return { TokenKind::End };
        }

    private:
        void skipSpace() {
            while(pos < src.length() && std::isspace(src[pos]))
                pos++;
        }

        const cial::String &src;
        size_t pos = 0;
    };

    class ExprParser {
    public:
        int eval(const cial::String &s) {
            lexer = std::make_unique<ExprLexer>(s);
            next();
            const int v = parseExpr(0).value;
            return v;
        }

        int getVar(const cial::String &n) {
            const auto it = vars.find(n);
            return it == vars.end() ? 0 : it->second;
        }

        void setVar(const cial::String &n, const int v) { vars[n] = v; }

    private:
        std::unique_ptr<ExprLexer> lexer;
        Token tok{};
        std::unordered_map<cial::String, int> vars;

        void next() { tok = lexer->next(); }

        static int lbp(const TokenKind k) {
            switch(k) {
                case TokenKind::Comma:
                    return 1;
                case TokenKind::Assign:
                    return 2;
                case TokenKind::OrOr:
                    return 3;
                case TokenKind::AndAnd:
                    return 4;
                case TokenKind::BitOr:
                    return 5;
                case TokenKind::BitXor:
                    return 6;
                case TokenKind::BitAnd:
                    return 7;
                case TokenKind::Equal:
                case TokenKind::NotEqual:
                    return 8;
                case TokenKind::LT:
                case TokenKind::GT:
                case TokenKind::LE:
                case TokenKind::GE:
                    return 9;
                case TokenKind::Plus:
                case TokenKind::Minus:
                    return 10;
                case TokenKind::Mul:
                case TokenKind::Div:
                case TokenKind::Mod:
                    return 11;
                default:
                    return 0;
            }
        }

        ExprValue parseExpr(const int rbp) {
            Token t = tok;
            next();

            ExprValue left = nud(t);

            while(rbp < lbp(tok.kind)) {
                t = tok;
                next();
                left = led(t, left);
            }
            return left;
        }

        ExprValue nud(const Token &t) {
            switch(t.kind) {
                case TokenKind::Symbol: {
                    const int v = getVar(t.text);
                    return ExprValue::l(t.text, v);
                }
                case TokenKind::Number:
                    return ExprValue::r(t.value);
                case TokenKind::Plus:
                    return ExprValue::r(+parseExpr(100).value);

                case TokenKind::Minus:
                    return ExprValue::r(-parseExpr(100).value);

                case TokenKind::Not:
                    return ExprValue::r(!parseExpr(100).value);
                case TokenKind::LParen: {
                    const ExprValue &v = parseExpr(0);
                    if(tok.kind != TokenKind::RParen)
                        throw std::runtime_error("expected ')'");
                    next();

                    // 括号破坏左值属性
                    return ExprValue::r(v.value);
                }

                default:
                    throw std::runtime_error("invalid expression");
            }
        }

        ExprValue led(const Token &t, const ExprValue &left) {
            switch(t.kind) {
                case TokenKind::Comma:
                    return parseExpr(1);
                case TokenKind::Assign: {
                    if(!left.isLValue)
                        throw std::runtime_error("left operand is not assignable");

                    // 右结合
                    const ExprValue &right = parseExpr(lbp(t.kind) - 1);

                    setVar(left.name, right.value);
                    return ExprValue::r(right.value);
                }

                case TokenKind::OrOr:
                    return ExprValue::r(left.value || parseExpr(lbp(t.kind)).value);
                case TokenKind::AndAnd:
                    return ExprValue::r(left.value && parseExpr(lbp(t.kind)).value);
                case TokenKind::BitOr:
                    return ExprValue::r(left.value | parseExpr(lbp(t.kind)).value);
                case TokenKind::BitXor:
                    return ExprValue::r(left.value ^ parseExpr(lbp(t.kind)).value);
                case TokenKind::BitAnd:
                    return ExprValue::r(left.value & parseExpr(lbp(t.kind)).value);
                case TokenKind::Equal:
                    return ExprValue::r(left.value == parseExpr(lbp(t.kind)).value);
                case TokenKind::NotEqual:
                    return ExprValue::r(left.value != parseExpr(lbp(t.kind)).value);
                case TokenKind::LT:
                    return ExprValue::r(left.value < parseExpr(lbp(t.kind)).value);
                case TokenKind::GT:
                    return ExprValue::r(left.value > parseExpr(lbp(t.kind)).value);
                case TokenKind::LE:
                    return ExprValue::r(left.value <= parseExpr(lbp(t.kind)).value);
                case TokenKind::GE:
                    return ExprValue::r(left.value >= parseExpr(lbp(t.kind)).value);
                case TokenKind::Plus:
                    return ExprValue::r(left.value + parseExpr(lbp(t.kind)).value);
                case TokenKind::Minus:
                    return ExprValue::r(left.value - parseExpr(lbp(t.kind)).value);
                case TokenKind::Mul:
                    return ExprValue::r(left.value * parseExpr(lbp(t.kind)).value);
                case TokenKind::Div: {
                    const ExprValue &r = parseExpr(lbp(t.kind));
                    if(r.value == 0)
                        throw std::runtime_error("divide by zero");
                    return ExprValue::r(left.value / r.value);
                }
                case TokenKind::Mod:
                    return ExprValue::r(left.value % parseExpr(lbp(t.kind)).value);
                default:
                    break;
            }
            throw std::runtime_error("invalid operator");
        }
    };
} // namespace

namespace cial {

    struct PreProcessor::Impl {
        ExprParser parser;
    };

    PreProcessor::PreProcessor() : _impl(new Impl) {}

    PreProcessor::~PreProcessor() noexcept { delete _impl; }

    void PreProcessor::onSet(const String &expr) const { _impl->parser.eval(expr); }

    void PreProcessor::onIf(const String &expr) {
        const bool cond = _impl->parser.eval(expr) != 0;
        const bool parent = _ifStack.empty() ? true : _ifStack.back();
        _ifStack.push_back(parent && cond);
    }

    void PreProcessor::onEndIf() {
        if(!_ifStack.empty())
            _ifStack.pop_back();
    }

    bool PreProcessor::isEnabled() const { return _ifStack.empty() ? true : _ifStack.back(); }

    int PreProcessor::getVar(const String &n) const { return _impl->parser.getVar(n); }

    void PreProcessor::setVar(const String &n, const int v) const { _impl->parser.setVar(n, v); }
} // namespace cial