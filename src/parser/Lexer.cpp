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

#include "types/Real.hpp"

#include <fmt/format.h>

using namespace cial::Syntax;

std::multimap<std::uint8_t, Lexer::LexerCaseCallable> Lexer::S_Cases = []() -> auto {
    std::multimap<std::uint8_t, LexerCaseCallable> map;

    for(char c : "[](){}~?:,#$")
        map.emplace(c, std::bind_front(&Lexer::singletonSign));

    // number literal
    for(char c : ".0123456789")
        map.emplace(c, std::bind_front(&Lexer::numberConstVal));

    // block comment, line comment
    map.emplace('/', std::bind_front(&Lexer::lineComment));
    map.emplace('/', std::bind_front(&Lexer::blockComment));
    map.emplace('/', std::bind_front(&Lexer::slash));
    map.emplace('\\', std::bind_front(&Lexer::backslash));

    map.emplace('=', std::bind_front(&Lexer::equalSign));
    map.emplace('!', std::bind_front(&Lexer::exclamationSign));
    map.emplace('&', std::bind_front(&Lexer::ampersandSign));
    map.emplace('|', std::bind_front(&Lexer::vertLineSign));

    map.emplace('.', std::bind_front(&Lexer::dotSign));

    map.emplace('-', std::bind_front(&Lexer::minus));

    map.emplace('+', std::bind_front(&Lexer::plus));

    map.emplace('*', std::bind_front(&Lexer::mul));

    // "> operator more..."
    map.emplace('>', std::bind_front(&Lexer::gtSign));

    // "<%" octet literal
    map.emplace('<', std::bind_front(&Lexer::octetLiteral));

    // "< operator more..."
    map.emplace('<', std::bind_front(&Lexer::ltSign));
    map.emplace('%', std::bind_front(&Lexer::percent));
    map.emplace('^', std::bind_front(&Lexer::chevron));

    // line terminator
    map.emplace(';', std::bind_front(&Lexer::lineTerminator));
    map.emplace('\'', std::bind_front(&Lexer::stringConstVal));
    map.emplace('"', std::bind_front(&Lexer::stringConstVal));
    map.emplace('@', std::bind_front(&Lexer::templateStringConstVal));

    return std::move(map);
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
    const size_t pos = _sourceFile.pos();
    if(pos == 0)
        return;

    // 最多回溯 4 个字节
    for(size_t i = 1; i <= 4 && i <= pos; ++i) {
        const uint8_t ch = _sourceFile[pos - i];

        // ASCII
        if((ch & 0x80) == 0) {
            _sourceFile.seek(pos - 1);
            return;
        }

        // UTF-8 起始字节
        if((ch & 0xE0) == 0xC0 || (ch & 0xF0) == 0xE0 || (ch & 0xF8) == 0xF0) {
            _sourceFile.seek(pos - i);
            return;
        }
    }

    // fallback
    _sourceFile.seek(pos - 1);
}

std::pair<uint32_t, uint32_t> Lexer::getCurrentRowCol() const {
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

        const auto isComment = token->type() == TokenType::LineComment || token->type() == TokenType::BlockComment;

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

bool Lexer::takeOverToken(Token &token) {
    if(_tokens.empty())
        return false;

    token = std::move(*_tokens.front());
    _tokens.pop_front();

    if(token.type() == TokenType::EndOfFile)
        return false;

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
bool Lexer::match(const String &literal) {
    _sourceFile.pushMark();
    DEFER { _sourceFile.popMark(); };

    // 实际可以少循环一次, 因为 ch 一定和 literal[0] 匹配
    return std::ranges::all_of(literal, [&](const auto targetCh) {
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
    if(match("/*"_str)) {
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
    std::stringstream stream{ "" };
    auto ch = read();
    constexpr std::string valid = ".0123456789Ee";
    int32_t shifting = 0;
    auto hasActualDigits = false;
    auto valueType = ValueType::Integer;

    while(valid.find_first_of(static_cast<char>(ch)) != std::string::npos) {
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
                stream.write(reinterpret_cast<const char *>(runeType.data), runeType.width);
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
        stream.write(reinterpret_cast<const char *>(runeType.data), runeType.width);
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

    const auto fixValue = [&](const double &val) {
        double ret = val;
        if(shifting > 0)
            ret *= pow(10, shifting);
        if(shifting < 0)
            ret /= pow(10, -shifting);
        return ret;
    };

    if(!stream.str().empty()) {
        if(valueType == ValueType::Real) {
            double val{ 0.0 };
            stream >> val;
            token = makeToken(TokenType::ConstVal, Real(fixValue(val)));
        }
        if(valueType == ValueType::Integer) {
            Integer val = 0;
            stream >> val;
            val = static_cast<Integer>(fixValue(static_cast<double>(val)));
            token = makeToken(TokenType::ConstVal, val);
        }
        return true;
    }
    return false;
}

// regex expr : 0x\\d*\.?\\d+[pP]\\d*
bool Lexer::parseNonDecimalNumber(Token *&token, std::stringstream &ss, std::int8_t (*validDigits)(char),
                                  const std::int8_t base) {
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

void Lexer::parseNonDecimalReal(Token *&token, const std::string &decimalStr, std::int8_t (*validDigits)(char),
                                const std::int8_t baseBits) {
    // parse non-decimal(hex decimal, octal or binary) floating-point number.
    // this routine heavily depends on IEEE double floating-point number expression.
    uint64_t main = 0ull; // significand
    int32_t exp = 0; // 2^n exponential
    int32_t numSignIf = 0; // significand bit count (including leading left-most '1') in "main"
    bool pointPassed = false;

    // scan input
    for(size_t i = 0; i < decimalStr.length(); i++) {
        if(const auto decimal = decimalStr[i]; decimal == '.') {
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
                    if(1 << b & n)
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
                        exp -= baseBits - b + 1;
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

    main >>= 64 - 1 - SIGNIFICAND_BITS;

    if(main == 0) {
        // zero
        token = makeToken(TokenType::ConstVal, Real(0.0));
        return;
    }

    main &= (1ull << SIGNIFICAND_BITS) - 1ull;

    if(exp < EXP_MIN) {
        // informal
        // treat as zero
        token = makeToken(TokenType::ConstVal, Real(0.0));
        return;
    }

    if(exp > EXP_MAX) {
        // too large
        // treat as infinity

        token = makeToken(TokenType::ConstVal, Real(P_INF));
        return;
    }

    Real temp{ 0.0 };

    // compose IEEE double
    *reinterpret_cast<Integer *>(&temp) =
        static_cast<Integer>(makeSign(false) | makeExponent(exp) | make_significand(main));

    token = makeToken(TokenType::ConstVal, temp);
}

bool Lexer::parseNonDecimalInteger(Token *&token, const std::string &decimalStr, int8_t (*validDigits)(char),
                                   const int8_t baseBits) {
    Integer v = 0;
    for(const auto decimal : decimalStr) {
        v <<= baseBits;
        v += validDigits(decimal);
    }
    token = makeToken(TokenType::ConstVal, v);
    return true;
}

// 如果是 . | p | P 那么isReal = true
void Lexer::extractNumber(std::int8_t (*validDigits)(char), const std::string &expMark, std::stringstream &ss,
                          bool &isReal) {
    // 小数点
    bool pointFound = false;
    // 指数
    bool expFound = false;

    const std::string valid = "+-.0123456789" + expMark;
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
                if(valid.find_first_of(ch) == std::string::npos)
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
    } while(valid.find_first_of(ch) != std::string::npos);

    isReal = pointFound || expFound;
}

bool Lexer::identifier(Token *&token) {
    const auto name = readIdentifier();

    if(name.isEmpty())
        return false;

    static auto Keywords = []() -> auto {
        std::unordered_map<String, Token> keywords{};
        keywords.emplace("true"_str, Token{ TokenType::ConstVal, 1 });
        keywords.emplace("false"_str, Token{ TokenType::ConstVal, 0 });
        keywords.emplace("Infinity"_str, Token{ TokenType::ConstVal, Real::negativeInf() });
        keywords.emplace("NaN"_str, Token{ TokenType::ConstVal, Real::signalingNan() });
        keywords.emplace("null"_str, Token{ TokenType::Null });

        keywords.emplace("function"_str, Token{ TokenType::Function });
        keywords.emplace("class"_str, Token{ TokenType::Class });
        keywords.emplace("extends"_str, Token{ TokenType::Extends });
        keywords.emplace("return"_str, Token{ TokenType::Return });

        keywords.emplace("var"_str, Token{ TokenType::Var });
        keywords.emplace("const"_str, Token{ TokenType::Const });

        keywords.emplace("if"_str, Token{ TokenType::If });
        keywords.emplace("else"_str, Token{ TokenType::Else });

        keywords.emplace("int"_str, Token{ TokenType::Int });
        keywords.emplace("real"_str, Token{ TokenType::Real });
        keywords.emplace("string"_str, Token{ TokenType::String });

        keywords.emplace("new"_str, Token{ TokenType::New });

        keywords.emplace("do"_str, Token{ TokenType::Do });
        keywords.emplace("while"_str, Token{ TokenType::While });
        keywords.emplace("for"_str, Token{ TokenType::For });
        keywords.emplace("continue"_str, Token{ TokenType::Continue });
        keywords.emplace("break"_str, Token{ TokenType::Break });

        keywords.emplace("switch"_str, Token{ TokenType::Switch });
        keywords.emplace("case"_str, Token{ TokenType::Case });
        keywords.emplace("default"_str, Token{ TokenType::Default });
        return std::move(keywords);
    }();

    // get keyword
    if(const auto it = Keywords.find(name); it != Keywords.end()) {
        token = makeToken(it->second);
        return true;
    }

    token = makeToken(TokenType::Identifier, String{ name });

    return true;
}

cial::String Lexer::readIdentifier() {
    auto ch = read(false);
    if(!isRuneLetter(ch)) {
        return ""_str;
    }
    std::stringstream stream{};

    auto runeType = utf8Encode(ch);
    stream.write(reinterpret_cast<const char *>(runeType.data), runeType.width);

    while(true) {
        ch = read(false);
        if(isRuneLetter(ch) || isRuneDigit(ch)) {
            runeType = utf8Encode(ch);
            stream.write(reinterpret_cast<const char *>(runeType.data), runeType.width);
            continue;
        }
        rewindOneChar();
        return String{ stream.str() };
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
        { "==="_str, TokenType::DiscEqual },
        { "=="_str, TokenType::Equal },
        { ","_str, TokenType::Comma }, // comma like perl
        { "="_str, TokenType::Assignment },
    };
    return boringMatch(token, signArr);
}

bool Lexer::plus(Token *&token) {
    static const OperatorTokenSet signArr{
        { "++"_str, TokenType::Increment },
        { "+="_str, TokenType::PlusEqual },
        { "+"_str, TokenType::Plus },
    };
    return boringMatch(token, signArr);
}

bool Lexer::minus(Token *&token) {
    static const OperatorTokenSet signArr{
        { "--"_str, TokenType::Decrement },
        { "-="_str, TokenType::MinusEqual },
        { "-"_str, TokenType::Minus },
    };
    return boringMatch(token, signArr);
}

bool Lexer::mul(Token *&token) {
    static const OperatorTokenSet signArr{
        { "*="_str, TokenType::AsteriskEqual },
        { "*"_str, TokenType::Asterisk },
    };
    return boringMatch(token, signArr);
}

bool Lexer::gtSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { ">>>="_str, TokenType::RBitShiftEqual },  { ">>>"_str, TokenType::RBitShift },
        { ">>="_str, TokenType::RArithShiftEqual }, { ">>"_str, TokenType::RArithShift },
        { ">="_str, TokenType::GtOrEqual },         { ">"_str, TokenType::Gt }
    };
    return boringMatch(token, signArr);
}


bool Lexer::ltSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "<<="_str, TokenType::LArithShiftEqual }, { "<->"_str, TokenType::Swap }, { "<="_str, TokenType::LtOrEqual },
        { "<<"_str, TokenType::LArithShift },       { "<"_str, TokenType::Lt },
    };
    return boringMatch(token, signArr);
}

bool Lexer::exclamationSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "!=="_str, TokenType::DiscNotEqual },
        { "!="_str, TokenType::NotEqual },
        { "!"_str, TokenType::Exclamation },
    };
    return boringMatch(token, signArr);
}


bool Lexer::ampersandSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "&&="_str, TokenType::LogicalAndEqual },
        { "&&"_str, TokenType::LogicalAnd },
        { "&="_str, TokenType::AmpersandEqual },
        { "&"_str, TokenType::Ampersand },
    };
    return boringMatch(token, signArr);
}

bool Lexer::vertLineSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "||="_str, TokenType::LogicalOrEqual },
        { "||"_str, TokenType::LogicalOr },
        { "|="_str, TokenType::VertLineEqual },
        { "|"_str, TokenType::VertLine },
    };
    return boringMatch(token, signArr);
}

bool Lexer::dotSign(Token *&token) {
    static const OperatorTokenSet signArr{
        { "..."_str, TokenType::Omit },
        { "."_str, TokenType::Dot },
    };
    return boringMatch(token, signArr);
}

bool Lexer::slash(Token *&token) {
    static const OperatorTokenSet signArr{
        { "/="_str, TokenType::SlashEqual },
        { "/"_str, TokenType::Slash },
    };
    return boringMatch(token, signArr);
}

bool Lexer::backslash(Token *&token) {
    static const OperatorTokenSet signArr{
        { "\\="_str, TokenType::BackslashEqual },
        { "\\"_str, TokenType::Backslash },
    };
    return boringMatch(token, signArr);
}

bool Lexer::percent(Token *&token) {
    static const OperatorTokenSet signArr{
        { "%="_str, TokenType::PercentEqual },
        { "%"_str, TokenType::Percent },
    };
    return boringMatch(token, signArr);
}

bool Lexer::chevron(Token *&token) {
    static const OperatorTokenSet signArr{
        { "^="_str, TokenType::ChevronEqual },
        { "^"_str, TokenType::Chevron },
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
    std::stringstream str{ "" };
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
                str.write(reinterpret_cast<const char *>(enRuneType.data), enRuneType.width);

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
                str.write(reinterpret_cast<const char *>(runeType.data), runeType.width);
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
        str.write(reinterpret_cast<const char *>(runeType.data), runeType.width);
    }

    token = makeToken(TokenType::ConstVal, String{ str.str() });

    return strPsState;
}

/**
 * 十六进制,字符序列
 */
bool Lexer::octetLiteral(Token *&token) {
    _sourceFile.pushMark();
    DEFER { _sourceFile.popMark(); };
    std::stringstream stream{ "" };
    std::vector<uint8_t> buf{};
    // parse an octet literal;
    // syntax is:
    // <% xx xx xx xx xx xx ... %>
    // where xx is hexadecimal 8bit(octet) binary representation.
    if(match("<%"_str)) {
        auto newSec = true;
        uint8_t oct = 0;

        for(;;) {
            skipComment();
            auto ch = read(false);
            if(ch == '%') {
                ch = read(false);
                if(ch == '>') {
                    token = makeToken(TokenType::ConstVal, Octet{ buf.data(), static_cast<std::uint32_t>(buf.size()) });
                    return true;
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
    return false;
}
