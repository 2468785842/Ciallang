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

#include "OperatorTrie.hpp"

#include "common/Defer.hpp"
#include "common/UTF8.hpp"

#include "types/Real.hpp"

using namespace cial::Syntax;

Lexer::Lexer(SourceFile &sourceFile, PreProcessor &preProcessor) :
    _sourceFile(sourceFile), _preProcessor(preProcessor) {}

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

        // ASCII 0xxx_xxxx 0x00 - 0x7F
        if((ch & 0x80) == 0) {
            _sourceFile.seek(pos - 1);
            return;
        }

        if((ch & 0xC0) == 0x80) {
            continue;
        }

        // UTF-8 start byte
        // 2 bytes 110x_xxxx 10xx_xxxx 0xC0 - 0xDF
        // 3 bytes 1110_xxxx 10xx_xxxx 10xx_xxxx 0xE0 - 0xEF
        // 4 bytes 1111_0xxx 10xx_xxxx 10xx_xxxx 10xx_xxxx 0xF0 - 0xF7
        if((ch & 0xE0) == 0xC0 || (ch & 0xF0) == 0xE0 || (ch & 0xF8) == 0xF0) {
            _sourceFile.seek(pos - i);
            return;
        }

        // bad char
        break;
    }

    // fallback
    _sourceFile.seek(pos - 1);
}

/**
 * 尝试从流, 读取一个Token
 *
 * @param r result
 * @param token 返回的Token, 会将数据填充
 * @return 是否匹配成功? succeed -> true
 *                     failed  -> false
 */
bool Lexer::next(Result &r, Token *&token) {

    DEFER { _hasNext = !_sourceFile.eof(); };

    // peek an unicode
    int32_t rune = read(r);

    if(rune == runeInvalid) {
        const LexemeGuard guard{ _sourceFile };
        token = makeToken(TokenType::Invalid);
        return guard.commit(*token, false);
    }

    if(rune == runeEof) {
        token = makeToken(TokenType::EndOfFile);

        const auto column = _sourceFile.columnByIndex(_sourceFile.length());

        const auto line = _sourceFile.lineByIndex(_sourceFile.length())->line;

        token->location.end(line, column);
        token->location.start(line, column);
        return true;
    }

    rewindOneChar();

    // preprocessor
    if(rune == '@') {
        while(!_sourceFile.eof()) {
            if(!_disablePreProcess && processor(r)) {
                continue;
            }
            break;
        }
    }

    // peek an unicode
    rune = read(r);

    if(rune == runeInvalid) {
        const LexemeGuard guard{ _sourceFile };
        token = makeToken(TokenType::Invalid);
        return guard.commit(*token, false);
    }

    if(rune == runeEof) {
        token = makeToken(TokenType::EndOfFile);

        const auto column = _sourceFile.columnByIndex(_sourceFile.length());

        const auto line = _sourceFile.lineByIndex(_sourceFile.length())->line;

        token->location.end(line, column);
        token->location.start(line, column);
        return true;
    }

    rewindOneChar();

    if(isRuneLetter(rune)) {
        // identifier
        const LexemeGuard guard{ _sourceFile };

        if(identifier(r, token))
            return guard.commit(*token);
    }

    if(rune < 0x80) {
        // this lexeme start row, col
        const LexemeGuard guard{ _sourceFile };

        if(rune >= '0' && rune <= '9') {
            if(numberConstVal(r, token))
                return guard.commit(*token);
            guard.restoreMark();
        }

        switch(static_cast<char>(rune)) {
            case '[':
            case ']':
            case '(':
            case ')':
            case '{':
            case '}':
            case '~':
            case '?':
            case ':':
            case ',':
            case '#':
            case '$':
            case '\\':
            case '=':
            case '!':
            case '&':
            case '|':
            case '-':
            case '+':
            case '*':
            case '>':
            case '%':
            case '^': {
                if(matchOperator(token))
                    return guard.commit(*token);
                // token = makeToken(TokenType::Error, String{});
                // return guard.commit(*token, false);
                break;
            }
            case '.':
                if(numberConstVal(r, token))
                    return guard.commit(*token);
                guard.restoreMark();
                if(matchOperator(token))
                    return guard.commit(*token);
                break;
            // block comment, line comment
            case '/':
                _disablePreProcess = true;
                if(lineComment(r, token)) {
                    _disablePreProcess = false;
                    return guard.commit(*token);
                }
                guard.restoreMark();
                if(blockComment(r, token)) {
                    _disablePreProcess = false;
                    return guard.commit(*token);
                }
                _disablePreProcess = false;
                guard.restoreMark();
                if(matchOperator(token))
                    return guard.commit(*token);
                break;
            case '<':
                if(octetLiteral(r, token))
                    return guard.commit(*token);
                guard.restoreMark();
                if(matchOperator(token))
                    return guard.commit(*token);
                break;
            case ';':
                if(lineTerminator(r, token))
                    return guard.commit(*token);
                break;
            case '\'':
            case '"':
                _disablePreProcess = true;
                if(stringConstVal(r, token)) {
                    _disablePreProcess = false;
                    return guard.commit(*token);
                }
                _disablePreProcess = false;
                break;
            case '@':
                _disablePreProcess = true;
                if(templateStringConstVal(r, token)) {
                    _disablePreProcess = false;
                    return guard.commit(*token);
                }
                _disablePreProcess = false;
                break;
            default:;
        }
    }

    const LexemeGuard guard{ _sourceFile };
    token = makeToken(TokenType::Invalid);
    read(r);
    return guard.commit(*token, false);
}

bool Lexer::readParenExpr(Result &r, std::string &out) const {
    int depth = 1;

    while(!_sourceFile.eof()) {
        const uint32_t c = read(r);
        if(c >= 0x80)
            return false;

        if(c == '(')
            depth++;
        else if(c == ')') {
            if(--depth == 0)
                return true;
        }

        out.push_back(static_cast<char>(c));
    }
    return false; // EOF before ')'
}

bool Lexer::processor(Result &r) const {

    if(read(r) == '@') {

        // ---------- @set ----------
        if(match(r, "set"_str)) {
            if(read(r) != '(')
                return false;

            std::string expr;
            if(!readParenExpr(r, expr))
                return false;

            _preProcessor.onSet(String{ expr });
            return true;
        }

        // ---------- @if ----------
        if(match(r, "if"_str)) {
            if(read(r) != '(')
                return false;

            std::string expr;
            if(!readParenExpr(r, expr))
                return false;

            _preProcessor.onIf(String{ expr });
            return true;
        }

        // ---------- @endif ----------
        if(match(r, "endif"_str)) {
            _preProcessor.onEndIf();
            return true;
        }
    } else {
        rewindOneChar();
    }

    // ---------- 普通脚本 ----------
    if(!_preProcessor.isEnabled()) {
        read(r);
        return true;
    }

    return false;
}

void Lexer::skipComment(Result &r) {
    Token *token{};
    const LexemeGuard guard{ _sourceFile };

    while(hasNext()) {
        if(!next(r, token)) {
            guard.restoreMark();
            break;
        }

        if(token->type() == TokenType::LineComment || token->type() == TokenType::BlockComment) {
            takeOverToken(*token);
            continue;
        }

        break;
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

/**
 * 从流中读取一个字符
 *
 * @param r result
 * @param skipWhitespace 是否跳过空格
 * @return 读取的字符
 */
int32_t Lexer::read(Result &r, const bool skipWhitespace) const {
    while(true) {
        const auto ch = _sourceFile.next(r);

        if(skipWhitespace && isRuneWhitespace(ch))
            continue;

        return ch;
    }
}

/**
 * 匹配一段文字,
 * N.B. the first char is definite: literal[0] == ch
 *
 * @param r result
 * @param literal 需要匹配的文字
 * @return 是否匹配成功? succeed -> true
 *                     failed  -> false
 */
bool Lexer::match(Result &r, const String &literal) const {
    _sourceFile.pushMark();
    DEFER { _sourceFile.popMark(); };

    return std::ranges::all_of(literal, [&](const char targetCh) {
        if(targetCh != read(r, false)) {
            _sourceFile.restoreTopMark();
            return false;
        }
        return true;
    });
}

bool Lexer::matchOperator(Token *&token) {
    static const OperatorTrie OpTrie{
#define LIST_TO_TOKEN_TYPE_PAIR(SYMBOL, NAME) { NAME##_str, TokenType::SYMBOL },
        SYMBOL_LIST(LIST_TO_TOKEN_TYPE_PAIR)
#undef LIST_TO_TOKEN_TYPE_PAIR
    };

    const size_t start = _sourceFile.pos();
    size_t i = start;

    int node = OpTrie.getRoot();
    int lastAccept = -1;
    size_t lastPos = start;

    while(i < _sourceFile.length()) {
        const std::uint8_t byte = _sourceFile[i];
        // not ASCII
        assert(byte < 0x80);
        const int next = OpTrie.getNode(node).next[byte];
        if(next == -1)
            break;

        node = next;
        ++i;

        if(OpTrie.getNode(node).token != TokenType::Invalid) {
            lastAccept = node;
            lastPos = i;
        }
    }

    if(lastAccept != -1) {
        _sourceFile.seek(lastPos);
        token = makeToken(OpTrie.getNode(lastAccept).token);
        return true;
    }

    return false;
}

bool Lexer::lineComment(Result &r, Token *&token) {
    if(match(r, "//"_str)) {
        token = makeToken(TokenType::LineComment);
        int32_t ch;
        do {
            ch = read(r, false);
        } while(ch != '\n' && !_sourceFile.eof());
        return true;
    }
    return false;
}

/**
 * 匹配块注释, 支持嵌套
 * @param r result msg
 * @param token 返回的token
 * @return 是否成功
 */
bool Lexer::blockComment(Result &r, Token *&token) {
    if(match(r, "/*"_str)) {
        auto blockCount = 1;

        while(!_sourceFile.eof()) {
            const int32_t ch = read(r);
            if(ch == '/' && read(r, false) == '*') {
                blockCount++;
                continue;
            }

            if(ch == '*' && read(r, false) == '/') {
                if(--blockCount == 0)
                    break;
            }
        }

        token = makeToken(TokenType::BlockComment);
        return true;
    }
    return false;
}

bool Lexer::numberConstVal(Result &r, Token *&token) {
    std::stringstream stream{ "" };
    auto ch = read(r);
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
            const auto tCh = read(r, false);
            if(tCh == 'x' || tCh == 'X')
                // 十六进制
                return parseNonDecimalNumber(r, token, stream, getHexNum, 4);
            if(tCh == 'b' || tCh == 'B')
                // 二进制
                return parseNonDecimalNumber(r, token, stream, getBinNum, 1);

            if(tCh == 'e' || tCh == 'E') {
                const auto runeType = utf8Encode(ch);
                stream.write(reinterpret_cast<const char *>(runeType.data), runeType.width);
                hasActualDigits = true;
                ch = tCh;
                continue;
            }
            if(isdigit(tCh) && tCh >= '0' && tCh <= '7')
                // octal, 八进制
                return parseNonDecimalNumber(r, token, stream, getOctNum, 3);

            rewindOneChar();
        }

        // 科学计数法
        if(ch == 'e' || ch == 'E') {
            const auto flag = read(r, false);

            if(flag != '-' && flag != '+')
                return false;

            int32_t bit = 0;
            auto num = read(r, false);
            while(isdigit(num)) {
                bit *= 10;
                bit += num - '0';
                num = read(r, false);
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
        ch = read(r, false);
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
bool Lexer::parseNonDecimalNumber(Result &r, Token *&token, std::stringstream &ss, std::int8_t (*validDigits)(char),
                                  const std::int8_t base) {
    bool isReal = false;
    extractNumber(r, validDigits, "Pp", ss, isReal);

    const auto &str = ss.str();
    if(str.empty())
        return false;

    if(isReal) {
        parseNonDecimalReal(r, token, str, validDigits, base);
        return true;
    }
    return parseNonDecimalInteger(r, token, str, validDigits, base);
}

void Lexer::parseNonDecimalReal(Result &r, Token *&token, const std::string &decimalStr,
                                std::int8_t (*validDigits)(char), const std::int8_t baseBits) {
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

bool Lexer::parseNonDecimalInteger(Result &r, Token *&token, const std::string &decimalStr, int8_t (*validDigits)(char),
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
void Lexer::extractNumber(Result &r, std::int8_t (*validDigits)(char), const std::string &expMark,
                          std::stringstream &ss, bool &isReal) const {
    // 小数点
    bool pointFound = false;
    // 指数
    bool expFound = false;

    const std::string valid = "+-.0123456789" + expMark;
    auto ch = static_cast<char>(read(r, false));
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
                ch = static_cast<char>(read(r));
                if(valid.find_first_of(ch) == std::string::npos)
                    break;
                if(ch == '+' || ch == '-') {
                    ss << ch;
                    // 跳过操作符,后面的所有空格
                    while(isRuneWhitespace(read(r))) {
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
        ch = static_cast<char>(read(r, false));
    } while(valid.find_first_of(ch) != std::string::npos);

    isReal = pointFound || expFound;
}

bool Lexer::identifier(Result &r, Token *&token) {
    const auto name = readIdentifier(r);

    if(name.isEmpty())
        return false;

    static std::unordered_map<String, Token> Keywords{
#define LIST_TO_TOKEN_PAIR(SYMBOL, NAME, VALUE) { NAME##_str, Token{ TokenType::SYMBOL, VALUE } },
        CONSTANT_VAL_LIST(LIST_TO_TOKEN_PAIR)
#undef LIST_TO_TOKEN_PAIR
#define LIST_TO_TOKEN_PAIR(SYMBOL, NAME) { NAME##_str, Token{ TokenType::SYMBOL } },
            KEYWORD_LIST(LIST_TO_TOKEN_PAIR)
#undef LIST_TO_TOKEN_PAIR
    };

    // get keyword
    if(const auto it = Keywords.find(name); it != Keywords.end()) {
        token = makeToken(it->second);
        return true;
    }

    token = makeToken(TokenType::Identifier, String{ name });

    return true;
}

cial::String Lexer::readIdentifier(Result &r) const {
    auto ch = read(r, false);
    if(!isRuneLetter(ch)) {
        return ""_str;
    }
    std::stringstream stream{};

    auto runeType = utf8Encode(ch);
    stream.write(reinterpret_cast<const char *>(runeType.data), runeType.width);

    while(true) {
        ch = read(r, false);
        if(isRuneLetter(ch) || isRuneDigit(ch)) {
            runeType = utf8Encode(ch);
            stream.write(reinterpret_cast<const char *>(runeType.data), runeType.width);
            continue;
        }
        rewindOneChar();
        return String{ stream.str() };
    }
}

bool Lexer::lineTerminator(Result &r, Token *&token) {
    const auto result = read(r) == ';';
    if(result)
        token = makeToken(TokenType::SemiColon);
    return result;
}

bool Lexer::stringConstVal(Result &r, Token *&token) {
    const int32_t delimiter = read(r, false);
    if(delimiter != '"' && delimiter != '\'') {
        rewindOneChar();
        return false;
    }
    return internalStringParser(r, token, static_cast<char>(delimiter)) == StringParseState::Delimiter;
}

/**
 * 需要多次匹配
 * such as @"this can be embeddable like &variable;"
 */
bool Lexer::templateStringConstVal(Result &r, Token *&token) {
    _sourceFile.pushMark();

    DEFER { _sourceFile.popMark(); };

    // read '@'
    auto ch = read(r);
    if(ch != '@') {
        rewindOneChar();
        return false;
    }
    ch = read(r);

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
            const auto result = next(r, token);
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
            const auto result = next(r, token);
            parseIndex = _sourceFile.pos();
            return result;
        }

        plusNeed = true;
        bool over;
        strPsState = internalStringParser(r, token, delimiter, &over, true);

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

StringParseState Lexer::internalStringParser(Result &r, Token *&token, const char delimiter, bool *templateOver,
                                             const bool templateMode) {
    std::stringstream str{ "" };
    auto strPsState = StringParseState::None;
    if(templateOver)
        *templateOver = false;
    for(;;) {
        int32_t ch = read(r, false);
        if(ch == runeEof) {
            rewindOneChar();
            break;
        }
        if(ch == '\\') {
            ch = read(r, false);
            if(ch == 'x' || ch == 'X') {
                // hex
                // starts with a "\x", be parsed while characters are
                // recognized as hex-characters, but limited of size of tjs_char.
                // on Windows, regex: \\x\d{5} ; will be parsed to UNICODE 16bit characters.        }
                ch = read(r, false);
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
                    ch = read(r, false);
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
                ch = read(r, false);
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
                    ch = read(r, false);
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
            ch = read(r, false);
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
            ch = read(r, false);
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
            ch = read(r, false);
            if(ch == runeEof) {
                rewindOneChar();
                break;
            }

            if(ch == '{') {
                ch = read(r, false);
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
bool Lexer::octetLiteral(Result &r, Token *&token) {
    _sourceFile.pushMark();
    std::stringstream stream{ "" };
    std::vector<uint8_t> buf{};
    // parse an octet literal;
    // syntax is:
    // <% xx xx xx xx xx xx ... %>
    // where xx is hexadecimal 8bit(octet) binary representation.
    if(match(r, "<%"_str)) {
        auto newSec = true;
        uint8_t oct = 0;

        for(;;) {
            skipComment(r);
            auto ch = read(r, false);
            if(ch == '%') {
                ch = read(r, false);
                if(ch == '>') {
                    token = makeToken(TokenType::ConstVal, Octet{ buf.data(), static_cast<std::uint32_t>(buf.size()) });
                    _sourceFile.popMark();
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
