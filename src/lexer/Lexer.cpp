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

#include "Lexer.hpp"

#include "common/Defer.hpp"
#include "common/UTF8.hpp"
#include "logging/Logger.hpp"
#include "types/Octet.hpp"
#include "types/Value.hpp"

#include "IEEETypes.hpp"
#include "types/String.hpp"

#include <fmt/format.h>

using namespace Ciallang::Syntax;

std::multimap<std::int32_t, Lexer::LexerCaseCallable> Lexer::S_Cases{
    // block comment, line comment
    { '/', bind_front(&Lexer::lineComment) },
    { '/', bind_front(&Lexer::blockComment) },
    { '/', bind_front(&Lexer::slash) },
    { '\\', bind_front(&Lexer::backslash) },

    { '=', bind_front(&Lexer::equalSign) },
    { '!', bind_front(&Lexer::exclamationSign) },
    { '&', bind_front(&Lexer::ampersandSign) },
    { '|', bind_front(&Lexer::vertLineSign) },

    { '.', bind_front(&Lexer::numberConstVal) },
    { '.', bind_front(&Lexer::dotSign) },

    { '-', bind_front(&Lexer::minus) },

    { '+', bind_front(&Lexer::plus) },

    { '*', bind_front(&Lexer::mul) },

    // "> operator more..."
    { '>', bind_front(&Lexer::gtSign) },

    // "<%" octet literal
    { '<', bind_front(&Lexer::octetLiteral) },

    // "< operator more..."
    { '<', bind_front(&Lexer::ltSign) },
    { '%', bind_front(&Lexer::percent) },
    { '^', bind_front(&Lexer::chevron) },
    { '[', bind_front(&Lexer::singletonSign) },
    { ']', bind_front(&Lexer::singletonSign) },
    { '(', bind_front(&Lexer::singletonSign) },
    { ')', bind_front(&Lexer::singletonSign) },
    { '~', bind_front(&Lexer::singletonSign) },
    { '?', bind_front(&Lexer::singletonSign) },
    { ':', bind_front(&Lexer::singletonSign) },
    { ',', bind_front(&Lexer::singletonSign) },
    { '{', bind_front(&Lexer::singletonSign) },
    { '}', bind_front(&Lexer::singletonSign) },
    { '#', bind_front(&Lexer::singletonSign) },
    { '$', bind_front(&Lexer::singletonSign) },

    // line terminator
    { ';', bind_front(&Lexer::lineTerminator) },
    { '\'', bind_front(&Lexer::stringConstVal) },
    { '"', bind_front(&Lexer::stringConstVal) },
    { '@', bind_front(&Lexer::templateStringConstVal) },
};

[[maybe_unused]] void *Lexer::S_LoadCases = [] {
    // number literal
    string numberMarks = "0123456789";
    for(auto &mark : numberMarks)
        S_Cases.emplace(mark, bind_front(&Lexer::numberConstVal));

    return nullptr;
}();

Lexer::Lexer(SourceFile &sourceFile) : _sourceFile(sourceFile) {}

bool Lexer::boringMatch(Token *&token, const OperatorTokenSet &signMap) {
    for(const auto &[sign, _token] : signMap) {
        if(match(sign)) {
            token = makeToken(_token);
            return true;
        }
    }
    return false;
}

bool Lexer::hasNext() const { return _hasNext; }

/**
 * 回溯一个字符
 */
void Lexer::rewindOneChar() const {
    const auto pos = _sourceFile.pos();
    if(pos == 0)
        return;
    if(pos > _sourceFile.length()) {
        _sourceFile.seek(pos - 1);
        return;
    }
    // utf-8
    for(uint8_t i = 1; i <= 4; ++i) {
        const auto ch = _sourceFile[pos - i];
        if(!(ch >> 7)) {
            _sourceFile.seek(pos - 1);
            return;
        }
        if((ch & 0xC0) == 0xC0) {
            _sourceFile.seek(pos - i);
            return;
        }
    }
}

pair<uint32_t, uint32_t> Lexer::getCurrentRowCol() const {
    return std::make_pair(_sourceFile.columnByIndex(_sourceFile.pos()),
                          _sourceFile.lineByIndex(_sourceFile.pos())->line);
}

void Lexer::setTokenLocation(Token *&token) const {
    const auto [column, line] = getCurrentRowCol();

    token->location.end(line, column);
    token->location.start(line, column);
}

/**
 * 尝试从流, 读取一个Token
 *
 * @param token 返回的Token, 会将数据填充
 * @return 是否匹配成功? succeed -> true
 *                     failed  -> false
 */
bool Lexer::next(Token *&token) {
    // 向前看一个字符
    const auto rune = read();

    DEFER {
        _sourceFile.popMark();
        _hasNext = rune != runeEof && token->type() != TokenType::Invalid;
        if(*token == TokenType::Invalid) {
            _result.error(fmt::format("unknown char: {}", static_cast<char>(rune)));
        }
    };

    if(rune == runeInvalid) {
        token = makeToken(TokenType::Invalid);
        setTokenLocation(token);
        return false;
    }

    if(rune == runeEof) {
        token = makeToken(TokenType::EndOfFile);

        const auto column = _sourceFile.columnByIndex(_sourceFile.length());

        const auto line = _sourceFile.lineByIndex(_sourceFile.length())->line;

        token->location.end(line, column);
        token->location.start(line, column);
        return true;
    }
    // 区分大小写
    // rune = rune > 0x80 ? rune : tolower(rune);

    // 指针rollback
    rewindOneChar();
    // save mark
    _sourceFile.pushMark();

    // 根据向前看的字符确定应该调用哪一个 match method
    auto [fst, snd] = S_Cases.equal_range(rune);
    for(auto it = fst; it != snd; ++it) {
        // this lexeme start row, col
        const auto [startColumn, startLine] = getCurrentRowCol();

        // invoke match method
        if(it->second(this, token)) {
            // this lexeme end row, col
            const auto [endColumn, endLine] = getCurrentRowCol();

            // save lexeme info
            token->location.start(startLine, startColumn);
            token->location.end(endLine, endColumn);
            return true;
        }

        // no match restore mark, match the next
        _sourceFile.restoreTopMark();
    }

    // identifier
    if(isRuneLetter(rune)) {
        const auto [startColumn, startLine] = getCurrentRowCol();

        if(identifier(token)) {
            const auto [endColumn, endLine] = getCurrentRowCol();

            token->location.start(startLine, startColumn);
            token->location.end(endLine, endColumn);
            return true;
        }

        // no match restore mark, match the next
        _sourceFile.restoreTopMark();
    }

    token = makeToken(TokenType::Invalid);
    setTokenLocation(token);

    return false;
}

/**
 * 跳过注释
 */
void Lexer::skipComment() {
    // 性能不好, 如果不是注释会回退,导致两次扫描完全没必要
    Token *token{ nullptr };

    while(true) {
        _sourceFile.pushMark();
        next(token);

        CLL_ASSERT(token != nullptr, "token is null");

        auto isComment = token->type() == TokenType::LineComment || token->type() == TokenType::BlockComment;

        _tokens.pop_back();

        if(!isComment) {
            _sourceFile.restoreTopMark();
            _sourceFile.popMark();
            _hasNext = true;
            return;
        }

        _sourceFile.popMark();
    }
}

bool Lexer::tackOverToken(Token &token) {
    CLL_ASSERT(!_tokens.empty(), "tokens vec is empty");

    token = *_tokens.front();

    if(token.type() == TokenType::EndOfFile)
        return false;

    delete _tokens.front();
    _tokens.erase(_tokens.begin());
    return true;
}

const Result &Lexer::result() const { return _result; }

/**
 * 从流中读取一个字符
 *
 * @param skipWhitespace 是否跳过空格
 * @return 读取的字符
 */
int32_t Lexer::read(const bool skipWhitespace) {
    while(true) {
        const auto ch = _sourceFile.next(_result);
        if(_result.isFailed())
            return runeInvalid;

        if(skipWhitespace && isRuneWhitespace(ch))
            continue;

        return ch;
    }
}

/**
 * 匹配一段文字,
 * N.B. the first char is definite: literal[0] == ch
 *
 * @param literal 需要匹配的文字
 * @return 是否匹配成功? succeed -> true
 *                     failed  -> false
 */
bool Lexer::match(const string &literal) {
    _sourceFile.pushMark();
    DEFER { _sourceFile.popMark(); };

    // 实际可以少循环一次, 因为 ch 一定和 literal[0] 匹配
    return ranges::all_of(literal, [&](const auto targetCh) {
        if(targetCh != read(false)) {
            _sourceFile.restoreTopMark();
            return false;
        }
        return true;
    });
}

bool Lexer::lineComment(Token *&token) {
    if(auto ch = read(); ch == '/') {
        ch = read(false);
        if(ch == '/') {
            // usually we don't need comment
            token = makeToken(TokenType::LineComment);
            do {
                ch = read(false);
            } while(ch != '\n' && ch != runeEof);
            rewindOneChar();
            return true;
        }
    }
    return false;
}

/**
 * 匹配块注释, 支持嵌套
 * @param token 返回的token
 * @return 是否成功
 */
bool Lexer::blockComment(Token *&token) {
    if(match("/*")) {
        auto block_count = 1;
        token = makeToken(TokenType::BlockComment);

        // std::stringstream stream{};
        while(true) {
            auto ch = read(false);
            if(ch == runeEof) {
                token = makeToken(TokenType::EndOfFile);
                setTokenLocation(token);
                return true;
            }

            if(ch == '/') {
                ch = read(false);
                if(ch == '*') {
                    block_count++;
                    continue;
                }
                rewindOneChar();
                ch = read(false);
            } else if(ch == '*') {
                ch = read(false);
                if(ch == '/') {
                    block_count--;
                    if(block_count == 0)
                        break;
                    continue;
                }
                rewindOneChar();
                ch = read(false);
            }
            // const auto runeType = utf8Encode(ch);
            // stream << runeType.data;
        }

        // token.value = stream.str();
        return true;
    }
    return false;
}

bool Lexer::numberConstVal(Token *&token) {
    std::stringstream stream{ std::string{} };
    auto ch = read();
    const std::string valid = ".0123456789Ee";
    int32_t shifting = 0;
    auto hasActualDigits = false;
    auto valueType = ValueType::Integer;

    while(valid.find_first_of(static_cast<char>(ch)) != string::npos) {
        if(ch == '.') {
            if(valueType == ValueType::Real) {
                token = makeToken(TokenType::Invalid);
                rewindOneChar();
                return false;
            }
            valueType = ValueType::Real;
        }

        // 进制检查
        if(!hasActualDigits && ch == '0') {
            const auto tCh = read(false);
            if(tCh == 'x' || tCh == 'X')
                // 十六进制
                return parseNonDecimalNumber(token, stream, getHexNum, 4);
            if(tCh == 'b' || tCh == 'B')
                // 二进制
                return parseNonDecimalNumber(token, stream, getBinNum, 1);

            if(tCh == 'e' || tCh == 'E') {
                const auto runeType = utf8Encode(ch);
                stream << runeType.data;
                hasActualDigits = true;
                ch = tCh;
                continue;
            }
            if(isdigit(tCh) && tCh >= '0' && tCh <= '7')
                // octal, 八进制
                return parseNonDecimalNumber(token, stream, getOctNum, 3);

            rewindOneChar();
        }

        // 科学计数法
        if(ch == 'e' || ch == 'E') {
            const auto flag = read(false);

            if(flag != '-' && flag != '+')
                return false;

            int32_t bit = 0;
            auto num = read(false);
            while(isdigit(num)) {
                bit *= 10;
                bit += num - '0';
                num = read(false);
            }
            if(flag == '-')
                shifting = -bit;
            if(flag == '+')
                shifting = bit;
            break;
        }

        const auto runeType = utf8Encode(ch);
        stream << runeType.data;
        if(isdigit(ch)) {
            hasActualDigits = true;
        }
        ch = read(false);
    }

    if(!hasActualDigits) {
        rewindOneChar();
        return false;
    }

    rewindOneChar();

    const auto fixValue = [&](const Real &val) {
        Real ret = val;
        if(shifting > 0)
            ret *= pow(10, shifting);
        if(shifting < 0)
            ret /= pow(10, -shifting);
        return ret;
    };

    if(!stream.str().empty()) {
        if(valueType == ValueType::Real) {
            Real val = 0.0;
            stream >> val;
            val = fixValue(val);
            token = makeToken(TokenType::ConstVal, createReal(val));
        }
        if(valueType == ValueType::Integer) {
            Integer val = 0;
            stream >> val;
            val = static_cast<Integer>(fixValue(static_cast<Real>(val)));
            token = makeToken(TokenType::ConstVal, createInteger(val));
        }
        return true;
    }
    return false;
}

// regex expr : 0x\\d*\.?\\d+[pP]\\d*
bool Lexer::parseNonDecimalNumber(Token *&token, stringstream &ss, int8_t (*validDigits)(char), const int8_t base) {
    bool isReal = false;
    extractNumber(validDigits, "Pp", ss, isReal);

    const auto &str = ss.str();
    if(str.empty())
        return false;

    if(isReal) {
        parseNonDecimalReal(token, str, validDigits, base);
        return true;
    }
    return parseNonDecimalInteger(token, str, validDigits, base);
}

void Lexer::parseNonDecimalReal(Token *&token, const string &decimalStr, int8_t (*validDigits)(char),
                                const int8_t baseBits) {
    // parse non-decimal(hex decimal, octal or binary) floating-point number.
    // this routine heavily depends on IEEE double floating-point number expression.
    uint64_t main = 0ull; // significand
    int32_t exp = 0; // 2^n exponential
    int32_t numSignIf = 0; // significand bit count (including leading left-most '1') in "main"
    bool pointPassed = false;

    // scan input
    for(size_t i = 0; i < decimalStr.length(); i++) {
        const auto decimal = decimalStr[i];
        if(decimal == '.') {
            pointPassed = true;
        } else if(decimal == 'p' || decimal == 'P') {
            // 匹配到p,指针向前移动,如果已经到末尾,退出
            if(decimalStr.length() - 1 < ++i)
                break;

            const auto flag = decimalStr[++i];

            bool biAssign = false;
            if(flag == '+')
                biAssign = false;

            if(flag == '-')
                biAssign = true;

            int32_t bias = 0;
            while(true) {
                bias *= 10;
                bias += decimalStr[i++] - '0';
                if(decimalStr.length() - 1 < i)
                    break;
            }
            if(biAssign)
                bias = -bias;
            exp += bias;
            break;
        } else {
            const auto n = validDigits(decimal);
            if(numSignIf == 0) {
                // find msb flag bit
                int32_t b = baseBits - 1;
                while(b >= 0) {
                    if((1 << b) & n)
                        break;
                    b--;
                }

                b++;
                if(b) {
                    // n is not zero
                    // place it to the main's msb
                    numSignIf = b;
                    main |= static_cast<uint64_t>(n) << (64 - numSignIf);
                    if(pointPassed)
                        exp -= (baseBits - b + 1);
                    else
                        exp = b - 1;
                } else {
                    // n is zero
                    if(pointPassed)
                        exp -= baseBits;
                }
            } else {
                // append to main
                if(numSignIf + baseBits < 64) {
                    numSignIf += baseBits;
                    main |= static_cast<uint64_t>(n) << (64 - numSignIf);
                }
                if(!pointPassed)
                    exp += baseBits;
            }
        }
    }

    main >>= (64 - 1 - IEEE::SIGNIFICAND_BITS);

    if(main == 0) {
        // zero
        token = makeToken(TokenType::ConstVal, createReal(0.0));
        return;
    }

    main &= (1ull << IEEE::SIGNIFICAND_BITS) - 1ull;

    if(exp < IEEE::EXP_MIN) {
        // informal
        // treat as zero
        token = makeToken(TokenType::ConstVal, createReal(0.0));
        return;
    }

    if(exp > IEEE::EXP_MAX) {
        // too large
        // treat as infinity

        token = makeToken(TokenType::ConstVal, createReal(static_cast<double>(IEEE::P_INF)));
        return;
    }

    Real temp = 0.0;

    // compose IEEE double
    *reinterpret_cast<Integer *>(&temp) =
        IEEE::make_sign(false) | IEEE::make_exponent(exp) | IEEE::make_significand(main);

    token = makeToken(TokenType::ConstVal, createReal(temp));
}

bool Lexer::parseNonDecimalInteger(Token *&token, const string &decimalStr, int8_t (*validDigits)(char),
                                   const int8_t baseBits) {
    int64_t v = 0;
    for(const auto decimal : decimalStr) {
        v <<= baseBits;
        v += validDigits(decimal);
    }
    token = makeToken(TokenType::ConstVal, createInteger(v));
    return true;
}

// 如果是 . | p | P 那么isReal = true
void Lexer::extractNumber(int8_t (*validDigits)(char), const string &expMark, stringstream &ss, bool &isReal) {
    // 小数点
    bool pointFound = false;
    // 指数
    bool expFound = false;

    const string valid = "+-.0123456789" + expMark;
    auto ch = static_cast<char>(read(false));
    do {
        if(!expFound) {
            if(validDigits(ch) != -1) {
                ss << ch;
            } else if(ch == '.' && !pointFound) {
                pointFound = true;
                ss << ch;
            } else if(ch == expMark[0] || ch == expMark[1]) {
                expFound = true;
                ss << ch;
                ch = static_cast<char>(read());
                if(valid.find_first_of(ch) == string::npos)
                    break;
                if(ch == '+' || ch == '-') {
                    ss << ch;
                    // 跳过操作符,后面的所有空格
                    while(isRuneWhitespace(read())) {
                    }
                    // 多读一个,回溯
                    rewindOneChar();
                    break;
                }
                rewindOneChar();
            }
        } else if(isdigit(ch)) {
            ss << ch;
        } else {
            break;
        }
        ch = static_cast<char>(read(false));
    } while(valid.find_first_of(ch) != string::npos);

    isReal = pointFound || expFound;
}

bool Lexer::identifier(Token *&token) {
    const auto name = readIdentifier();

    if(name.empty())
        return false;

    static std::unordered_map<std::string, Token> Keywords{
        { "true", Token{ TokenType::ConstVal, createInteger(1) } },
        { "false", Token{ TokenType::ConstVal, createInteger(0) } },
        { "Infinity", Token{ TokenType::ConstVal, createReal(IEEE::Double::negative_infinity().value()) } },
        { "NaN", Token{ TokenType::ConstVal, createReal(IEEE::Double::signaling_nan().value()) } },

        { "function", Token{ TokenType::Function } },
        { "class", Token{ TokenType::Class } },
        { "extends", Token{ TokenType::Extends } },
        { "return", Token{ TokenType::Return } },

        { "var", Token{ TokenType::Var } },
        { "const", Token{ TokenType::Const } },

        { "if", Token{ TokenType::If } },
        { "else", Token{ TokenType::Else } },

        { "int", Token{ TokenType::Int } },
        { "real", Token{ TokenType::Real } },
        { "string", Token{ TokenType::String } },

        { "new", Token{ TokenType::New } },

        { "do", Token{ TokenType::Do } },
        { "while", Token{ TokenType::While } },
        { "for", Token{ TokenType::For } },
        { "break", Token{ TokenType::Break } },
        { "continue", Token{ TokenType::Continue } }
    };

    // get keyword
    auto it = Keywords.find(name);
    if(it != Keywords.end()) {
        token = makeToken(it->second);
        return true;
    }

    token = makeToken(TokenType::Identifier, createString(name.c_str(), name.size()));
    return true;
}

std::string Lexer::readIdentifier() {
    auto ch = read(false);
    if(!isRuneLetter(ch)) {
        return "";
    }
    std::stringstream stream{};

    auto runeType = utf8Encode(ch);
    stream << runeType.data;

    while(true) {
        ch = read(false);
        if(isRuneLetter(ch) || isRuneDigit(ch)) {
            runeType = utf8Encode(ch);
            stream << runeType.data;
            continue;
        }
        rewindOneChar();
        return stream.str();
    }
}

bool Lexer::lineTerminator(Token *&token) {
    const auto r = read() == ';';
    if(r)
        token = makeToken(TokenType::SemiColon);
    return r;
}

bool Lexer::equalSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "===", TokenType::DiscEqual },
        { "==", TokenType::Equal },
        { ",", TokenType::Comma }, // comma like perl
        { "=", TokenType::Assignment },
    };
    return boringMatch(token, signArr);
}

bool Lexer::plus(Token *&token) {
    static const OperatorTokenSet signArr{
        { "++", TokenType::Increment },
        { "+=", TokenType::PlusEqual },
        { "+", TokenType::Plus },
    };
    return boringMatch(token, signArr);
}

bool Lexer::minus(Token *&token) {
    static const OperatorTokenSet signArr{
        { "--", TokenType::Decrement },
        { "-=", TokenType::MinusEqual },
        { "-", TokenType::Minus },
    };
    return boringMatch(token, signArr);
}

bool Lexer::mul(Token *&token) {
    static const OperatorTokenSet signArr{
        { "*=", TokenType::AsteriskEqual },
        { "*", TokenType::Asterisk },
    };
    return boringMatch(token, signArr);
}

bool Lexer::gtSign(Token *&token) {
    static const OperatorTokenSet signArr{ { ">>>=", TokenType::RBitShiftEqual },  { ">>>", TokenType::RBitShift },
                                           { ">>=", TokenType::RArithShiftEqual }, { ">>", TokenType::RArithShift },
                                           { ">=", TokenType::GtOrEqual },         { ">", TokenType::Gt } };
    return boringMatch(token, signArr);
}


bool Lexer::ltSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "<<=", TokenType::LArithShiftEqual }, { "<->", TokenType::Swap }, { "<=", TokenType::LtOrEqual },
        { "<<", TokenType::LArithShift },       { "<", TokenType::Lt },
    };
    return boringMatch(token, signArr);
}

bool Lexer::exclamationSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "!==", TokenType::DiscNotEqual },
        { "!=", TokenType::NotEqual },
        { "!", TokenType::Exclamation },
    };
    return boringMatch(token, signArr);
}


bool Lexer::ampersandSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "&&=", TokenType::LogicalAndEqual },
        { "&&", TokenType::LogicalAnd },
        { "&=", TokenType::AmpersandEqual },
        { "&", TokenType::Ampersand },
    };
    return boringMatch(token, signArr);
}

bool Lexer::vertLineSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "||=", TokenType::LogicalOrEqual },
        { "||", TokenType::LogicalOr },
        { "|=", TokenType::VertLineEqual },
        { "|", TokenType::VertLine },
    };
    return boringMatch(token, signArr);
}

bool Lexer::dotSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "...", TokenType::Omit },
        { ".", TokenType::Dot },
    };
    return boringMatch(token, signArr);
}

bool Lexer::slash(Token *&token) {
    static const OperatorTokenSet signArr{
        { "/=", TokenType::SlashEqual },
        { "/", TokenType::Slash },
    };
    return boringMatch(token, signArr);
}

bool Lexer::backslash(Token *&token) {
    static const OperatorTokenSet signArr{
        { "\\=", TokenType::BackslashEqual },
        { "\\", TokenType::Backslash },
    };
    return boringMatch(token, signArr);
}

bool Lexer::percent(Token *&token) {
    static const OperatorTokenSet signArr{
        { "%=", TokenType::PercentEqual },
        { "%", TokenType::Percent },
    };
    return boringMatch(token, signArr);
}

bool Lexer::chevron(Token *&token) {
    static const OperatorTokenSet signArr{
        { "^=", TokenType::ChevronEqual },
        { "^", TokenType::Chevron },
    };
    return boringMatch(token, signArr);
}

bool Lexer::singletonSign(Token *&token) {
    auto r = true;
    switch(const auto ch = read(); ch) {
        case '[':
            token = makeToken(TokenType::LBracket);
            break;
        case ']':
            token = makeToken(TokenType::RBracket);
            break;
        case '(':
            token = makeToken(TokenType::LParenthesis);
            break;
        case ')':
            token = makeToken(TokenType::RParenthesis);
            break;
        case '~':
            token = makeToken(TokenType::Tilde);
            break;
        case '?':
            token = makeToken(TokenType::Question);
            break;
        case ':':
            token = makeToken(TokenType::Colon);
            break;
        case ',':
            token = makeToken(TokenType::Comma);
            break;
        case '{':
            token = makeToken(TokenType::LeftCurlyBrace);
            break;
        case '}':
            token = makeToken(TokenType::RightCurlyBrace);
            break;
        case '#':
            token = makeToken(TokenType::Sharp);
            break;
        case '$':
            token = makeToken(TokenType::Dollar);
            break;
        default:
            rewindOneChar();
            r = false;
    }
    return r;
}

bool Lexer::stringConstVal(Token *&token) {
    const int32_t delimiter = read(false);
    if(delimiter != '"' && delimiter != '\'') {
        rewindOneChar();
        return false;
    }
    return internalStringParser(token, static_cast<char>(delimiter)) == StringParseState::Delimiter;
}

/**
 * 需要多次匹配
 * such as @"this can be embeddable like &variable;"
 */
bool Lexer::templateStringConstVal(Token *&token) {
    _sourceFile.pushMark();

    DEFER { _sourceFile.popMark(); };

    // read '@'
    auto ch = read();
    if(ch != '@') {
        rewindOneChar();
        return false;
    }
    ch = read();

    if(ch == '\'' || ch == '"') {
        static size_t parseIndex = -1;
        static int32_t bracketPairCount = 0;
        static char delimiter = -1;
        static bool dollarRepl = false;
        static bool bracketPairNeed = true;
        static bool bracketPairNeedClose = false;
        static bool plusNeed = false;
        static auto strPsState = StringParseState::None;

        if(delimiter == -1) {
            parseIndex = _sourceFile.pos();
            delimiter = static_cast<char>(ch);
        }
        _sourceFile.seek(parseIndex);

        // )
        if(bracketPairCount != 0 && bracketPairNeedClose) {
            token = makeToken(TokenType::RParenthesis);
            bracketPairNeedClose = false;
            --bracketPairCount;

            if(bracketPairCount != 0) {
                _sourceFile.restoreTopMark();
            } else {
                // 结束匹配
                _sourceFile.seek(parseIndex);
                parseIndex = -1;
                delimiter = -1;
                dollarRepl = false;
                bracketPairNeed = true;
                plusNeed = false;
            }
            return true;
        }
        if(plusNeed) {
            token = makeToken(TokenType::Plus);
            plusNeed = false;
            _sourceFile.restoreTopMark();
            return true;
        }

        // (
        if(bracketPairNeed) {
            token = makeToken(TokenType::LParenthesis);
            bracketPairNeed = false;
            ++bracketPairCount;
            _sourceFile.restoreTopMark();
            return true;
        }

        // & 和 ${
        if(strPsState == StringParseState::Dollar || strPsState == StringParseState::Ampersand) {
            const auto result = next(token);
            // ${}替换符号结束
            if(strPsState == StringParseState::Dollar)
                dollarRepl = true;

            parseIndex = _sourceFile.pos();
            _sourceFile.restoreTopMark();

            if(strPsState == StringParseState::Ampersand) {
                bracketPairNeedClose = true;
                plusNeed = true;
                strPsState = StringParseState::None;
            }

            if(token->type() == TokenType::RightCurlyBrace && dollarRepl) {
                dollarRepl = false;
                token = makeToken(TokenType::RParenthesis);
                bracketPairNeedClose = false;
                --bracketPairCount;
                strPsState = StringParseState::None;
                plusNeed = true;
                return true;
            }

            return result;
        }

        if(dollarRepl) {
            const auto result = next(token);
            parseIndex = _sourceFile.pos();
            return result;
        }

        plusNeed = true;
        bool over;
        strPsState = internalStringParser(token, delimiter, &over, true);

        parseIndex = _sourceFile.pos() - 1;

        // str
        if(strPsState == StringParseState::Delimiter) {
            if(over) {
                // 模板字符串匹配完成
                bracketPairNeedClose = true;
                _sourceFile.restoreTopMark();
                return true;
            }
            _sourceFile.restoreTopMark();
            return true;
        }

        if(strPsState == StringParseState::None) {
            return false;
        }

        bracketPairNeed = true;
        _sourceFile.restoreTopMark();
        return true;
    }
    return false;
}

StringParseState Lexer::internalStringParser(Token *&token, const char delimiter, bool *templateOver,
                                             const bool templateMode) {
    stringstream str{ string{} };
    auto strPsState = StringParseState::None;
    if(templateOver)
        *templateOver = false;
    for(;;) {
        int32_t ch = read(false);
        if(ch == runeEof) {
            rewindOneChar();
            break;
        }
        if(ch == '\\') {
            ch = read(false);
            if(ch == 'x' || ch == 'X') {
                // hex
                // starts with a "\x", be parsed while characters are
                // recognized as hex-characters, but limited of size of tjs_char.
                // on Windows, regex: \\x\d{5} ; will be parsed to UNICODE 16bit characters.        }
                ch = read(false);
                if(ch == runeEof) {
                    rewindOneChar();
                    break;
                }

                int32_t code = 0, count = 0;
                auto hex = getHexNum(static_cast<char>(ch));
                while(hex != -1 && count < sizeof(int32_t) * 2) {
                    // code * 16
                    code <<= 4;
                    code += hex;
                    count++;
                    ch = read(false);
                    hex = getHexNum(static_cast<char>(ch));
                }
                rewindOneChar();
                if(ch == runeEof)
                    break;
                if(code == 0)
                    continue;

                // 将Unicode转为utf-8, 存储到窄字符序列
                const auto enRuneType = utf8Encode(code);
                str << enRuneType.data;

                continue;
            }
            if(ch == '0') {
                // octal
                ch = read(false);
                if(ch == runeEof) {
                    rewindOneChar();
                    break;
                }
                int32_t code = 0;
                auto oct = getOctNum(static_cast<char>(ch));
                while(oct != -1) {
                    // code * 8
                    code <<= 3;
                    code += oct;
                    ch = read(false);
                    if(ch == runeEof) {
                        rewindOneChar();
                        break;
                    }
                    oct = getOctNum(static_cast<char>(ch));
                }
                EncodedRuneType runeType;
                if(code != 0)
                    runeType = utf8Encode(code);
                else
                    runeType = utf8Encode(ch);
                str << runeType.data;
                continue;
            }
            str << unescapeBackSlash(static_cast<char>(ch));
            continue;
        }
        if(ch == delimiter) {
            // string
            ch = read(false);
            if(ch == runeEof) {
                rewindOneChar();
                strPsState = StringParseState::Delimiter;
                if(templateOver)
                    *templateOver = true;
                break;
            }
            // sequence of 'A' 'B' will be combined as 'AB'
            if(ch == delimiter)
                continue;
            strPsState = StringParseState::Delimiter;
            if(templateOver)
                *templateOver = true;

            rewindOneChar();
            break;
        }
        if(templateMode && ch == '&') {
            ch = read(false);
            if(ch == runeEof) {
                rewindOneChar();
                break;
            }
            strPsState = StringParseState::Ampersand;
            break;
        }
        if(templateMode && ch == '$') {
            // '$'
            // '{' must be placed immediately after '$'
            ch = read(false);
            if(ch == runeEof) {
                rewindOneChar();
                break;
            }

            if(ch == '{') {
                ch = read(false);
                if(ch == runeEof) {
                    rewindOneChar();
                    break;
                }

                strPsState = StringParseState::Dollar;
                break;
            }
            break;
        }

        const auto runeType = utf8Encode(ch);
        str << runeType.data;
    }

    token = makeToken(TokenType::ConstVal, createString(str.str().c_str(), str.str().size()));

    return strPsState;
}

/**
 * 十六进制,字符序列
 */
bool Lexer::octetLiteral(Token *&token) {
    _sourceFile.pushMark();
    DEFER { _sourceFile.popMark(); };
    stringstream stream{ string{} };
    vector<uint8_t> buf{};
    // parse an octet literal;
    // syntax is:
    // <% xx xx xx xx xx xx ... %>
    // where xx is hexadecimal 8bit(octet) binary representation.
    if(match("<%")) {
        auto newSec = true;
        uint8_t oct = 0;

        for(;;) {
            skipComment();
            auto ch = read(false);
            if(ch == '%') {
                ch = read(false);
                if(ch == '>') {
                    token = makeToken(TokenType::ConstVal, createOctet(buf.data(), buf.size()));
                    return true;
                }
                _sourceFile.restoreTopMark();
                return false;
            }

            ch = static_cast<uint8_t>(getHexNum(static_cast<char>(ch)));
            if(ch != -1) {
                if(newSec) {
                    oct = ch;
                    newSec = ch == ',';

                    if(newSec)
                        buf.push_back(oct);
                } else {
                    oct <<= 4;
                    oct += ch;

                    buf.push_back(oct);
                    newSec = true;
                }
            }
        }
    }
    // S_ConstVals;
    return false;
}
