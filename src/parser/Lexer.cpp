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

namespace cial::Syntax {

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
            const u8 ch = _sourceFile[pos - i];

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
     * @param token 返回的Token, 会将数据填充
     * @param enablePreProcessor 是否启用预处理?
     * @return 是否匹配成功? succeed -> true
     *                     failed  -> false
     */
    bool Lexer::next(Token *&token, const bool enablePreProcessor) {

        DEFER { _hasNext = !_sourceFile.eof(); };

        i32 rune = read(!tmplStrCtx.active);

        if(rune == runeInvalid) {
            token = makeToken(TokenType::Invalid);
            const auto [line, column] = getRowCol(_sourceFile.pos());
            token->location.end(line, column);
            token->location.start(line, column);
            return false;
        }

        if(rune == runeEof) {
            token = makeToken(TokenType::EndOfFile);
            const auto [line, column] = getRowCol(_sourceFile.length());
            token->location.end(line, column);
            token->location.start(line, column);
            return true;
        }

        rewindOneChar();

        if(tmplStrCtx.active) {
            const auto start = getRowCol(_sourceFile.pos());
            const bool r = templateStringConstVal(token);
            const auto end = getRowCol(_sourceFile.pos() - 1);
            patchTokenLoc(*token, start, end);
            return r;
        }

        // preprocessor
        if(enablePreProcessor && rune == '@') {
            while(!_sourceFile.eof()) {
                if(processor()) {
                    continue;
                }
                break;
            }

            rune = read();

            if(rune == runeInvalid) {
                token = makeToken(TokenType::Invalid);
                const auto [line, column] = getRowCol(_sourceFile.pos());
                token->location.end(line, column);
                token->location.start(line, column);
                return false;
            }

            if(rune == runeEof) {
                token = makeToken(TokenType::EndOfFile);
                const auto [line, column] = getRowCol(_sourceFile.length());
                token->location.end(line, column);
                token->location.start(line, column);
                return true;
            }

            rewindOneChar();
        }

        // identifier
        if(isRuneLetter(rune)) {
            const auto start = getRowCol(_sourceFile.pos());
            identifier(token);
            const auto end = getRowCol(_sourceFile.pos() - 1);
            patchTokenLoc(*token, start, end);
            return true;
        }

        if(rune < 0x80) {
            if(rune >= '0' && rune <= '9') {
                const auto start = getRowCol(_sourceFile.pos());
                const bool r = numberConstVal(token);
                const auto end = getRowCol(_sourceFile.pos() - 1);
                patchTokenLoc(*token, start, end);
                return r;
            }

            switch(rune) {
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
                    const auto start = getRowCol(_sourceFile.pos());
                    const bool r = matchOperator(token);
                    const auto end = getRowCol(_sourceFile.pos() - 1);
                    patchTokenLoc(*token, start, end);
                    return r;
                }
                case '.': {
                    const auto start = getRowCol(_sourceFile.pos());
                    read(false); // '.'
                    const i32 peek = read(false);
                    rewindOneChar();
                    rewindOneChar();

                    bool r;
                    if(isRuneDigit(peek)) {
                        r = numberConstVal(token);
                    } else {
                        r = matchOperator(token);
                    }
                    const auto end = getRowCol(_sourceFile.pos() - 1);
                    patchTokenLoc(*token, start, end);
                    return r;
                }
                // block comment, line comment
                case '/': {
                    const auto start = getRowCol(_sourceFile.pos());
                    read(false); // '/'
                    const u32 peek = read(false);

                    rewindOneChar();
                    rewindOneChar();

                    bool r;
                    if(peek == '/') {
                        r = lineComment(token);
                    } else if(peek == '*') {
                        r = blockComment(token);
                    } else {
                        r = matchOperator(token);
                    }
                    const auto end = getRowCol(_sourceFile.pos() - 1);
                    patchTokenLoc(*token, start, end);
                    return r;
                }
                case '<': {
                    const auto start = getRowCol(_sourceFile.pos());
                    read(false); // '<'
                    const u32 peek = read(false);

                    rewindOneChar();
                    rewindOneChar();

                    bool r;
                    if(peek == '%') {
                        r = octetLiteral(token);
                    } else {
                        r = matchOperator(token);
                    }
                    const auto end = getRowCol(_sourceFile.pos() - 1);
                    patchTokenLoc(*token, start, end);
                    return r;
                }
                case ';': {
                    const auto start = getRowCol(_sourceFile.pos());
                    read();
                    token = makeToken(TokenType::SemiColon);
                    const auto end = getRowCol(_sourceFile.pos() - 1);
                    patchTokenLoc(*token, start, end);
                    return true;
                }
                case '\'':
                case '"': {
                    const auto start = getRowCol(_sourceFile.pos());
                    const bool r = stringConstVal(token);
                    const auto end = getRowCol(_sourceFile.pos() - 1);
                    patchTokenLoc(*token, start, end);
                    return r;
                }
                case '@': {
                    const auto start = getRowCol(_sourceFile.pos());
                    const bool r = templateStringConstVal(token);
                    const auto end = getRowCol(_sourceFile.pos() - 1);
                    patchTokenLoc(*token, start, end);
                    return r;
                }
                default: {
                    const auto start = getRowCol(_sourceFile.pos());
                    read();
                    token = makeToken(TokenType::Error,
                                      String{ fmt::format("unexpected character '{}'", static_cast<char>(rune)) });
                    const auto end = getRowCol(_sourceFile.pos() - 1);
                    patchTokenLoc(*token, start, end);
                    return false;
                }
            }
        }

        const auto start = getRowCol(_sourceFile.pos());
        read();
        token = makeToken(TokenType::Invalid);
        const auto end = getRowCol(_sourceFile.pos() - 1);
        patchTokenLoc(*token, start, end);
        return false;
    }

    bool Lexer::readPreProcessorExpr(std::string &out) {
        int depth = 1;

        while(!_sourceFile.eof()) {
            const u32 c = read();
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

    bool Lexer::processor() {

        if(read() == '@') {

            // ---------- @set ----------
            if(match("set"_str)) {
                if(read() != '(')
                    return false;

                std::string expr;
                if(!readPreProcessorExpr(expr))
                    return false;

                _preProcessor.onSet(String{ expr });
                return true;
            }

            // ---------- @if ----------
            if(match("if"_str)) {
                if(read() != '(')
                    return false;

                std::string expr;
                if(!readPreProcessorExpr(expr))
                    return false;

                _preProcessor.onIf(String{ expr });
                return true;
            }

            // ---------- @endif ----------
            if(match("endif"_str)) {
                _preProcessor.onEndIf();
                return true;
            }
        }
        rewindOneChar();


        // ---------- 普通脚本 ----------
        if(!_preProcessor.isEnabled()) {
            read();
            return true;
        }

        return false;
    }

    void Lexer::skipComment() {
        Token *token{};

        while(hasNext()) {
            if(!next(token)) {
                break;
            }

            if(token->type() == TokenType::LineComment || token->type() == TokenType::BlockComment) {
                Token ignoreToken;
                takeOverToken(ignoreToken);
                continue;
            }

            break;
        }
    }

    bool Lexer::takeOverToken(Token &token) {
        assert(!_tokens.empty());

        Token *tmp = _tokens.front();
        token = std::move(*tmp);
        _tokens.pop_front();
        delete tmp;

        if(token.type() == TokenType::EndOfFile)
            return false;

        return true;
    }

    /**
     * 从流中读取一个字符
     *
     * @param skipWhitespace 是否跳过空格
     * @return 读取的字符
     */
    i32 Lexer::read(const bool skipWhitespace) {
        while(true) {
            const auto ch = _sourceFile.next(_r);

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

        return std::ranges::all_of(literal, [&](const char targetCh) {
            if(targetCh != read(false)) {
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
            const u8 byte = _sourceFile[i];
            // not ASCII
            if(byte >= 0x80)
                break;
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

        _sourceFile.seek(lastPos);

        if(lastAccept != -1) {
            token = makeToken(OpTrie.getNode(lastAccept).token);
            return true;
        }

        token = makeToken(TokenType::Error, "expected a operator character"_str);
        return false;
    }

    bool Lexer::lineComment(Token *&token) {
        if(match("//"_str)) {
            token = makeToken(TokenType::LineComment);
            i32 ch;
            do {
                ch = read(false);
                if(ch == runeEof) {
                    rewindOneChar();
                    break;
                }
            } while(ch != '\n');
            return true;
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
            auto blockCount = 1;

            while(!_sourceFile.eof()) {
                const i32 ch = read();
                if(ch == '/' && read(false) == '*') {
                    blockCount++;
                    continue;
                }

                if(ch == '*' && read(false) == '/') {
                    if(--blockCount == 0)
                        break;
                }
            }

            token = makeToken(TokenType::BlockComment);
            return true;
        }
        return false;
    }

    bool Lexer::numberConstVal(Token *&token) {
        std::stringstream stream{ "" };
        auto ch = read();
        constexpr std::string valid = ".0123456789Ee";
        i32 shifting = 0;
        auto hasActualDigits = false;
        auto valueType = ValueType::Integer;

        while(valid.find_first_of(static_cast<char>(ch)) != std::string::npos) {
            if(ch == '.') {
                if(valueType == ValueType::Real) {
                    token = makeToken(TokenType::Error, "unexpected second decimal point"_str);
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

                if(flag != '-' && flag != '+') {
                    token = makeToken(TokenType::Error, "scientific notation expected character '-' or '+'"_str);
                    return false;
                }

                i32 bit = 0;
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

        rewindOneChar();

        if(!hasActualDigits) {
            token = makeToken(TokenType::Error, "number value expected digits"_str);
            return false;
        }

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

        token = makeToken(TokenType::Error, "expected a number value"_str);
        return false;
    }

    // regex expr : 0x\\d*\.?\\d+[pP]\\d*
    bool Lexer::parseNonDecimalNumber(Token *&token, std::stringstream &ss, i8 (*validDigits)(char), const i8 base) {
        bool isReal = false;
        _sourceFile.pushMark();
        DEFER { _sourceFile.popMark(); };
        extractNumber(validDigits, "Pp", ss, isReal);

        const auto &str = ss.str();
        if(str.empty()) {
            _sourceFile.restoreTopMark();
            token = makeToken(TokenType::Error, "unexcepted exponent"_str);
            return false;
        }

        if(isReal) {
            parseNonDecimalReal(token, str, validDigits, base);
            return true;
        }
        return parseNonDecimalInteger(token, str, validDigits, base);
    }

    void Lexer::parseNonDecimalReal(Token *&token, const std::string &decimalStr, i8 (*validDigits)(char),
                                    const i8 baseBits) {
        // parse non-decimal(hex decimal, octal or binary) floating-point number.
        // this routine heavily depends on IEEE double floating-point number expression.
        u64 main = 0ull; // significand
        i32 exp = 0; // 2^n exponential
        i32 numSignIf = 0; // significand bit count (including leading left-most '1') in "main"
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

                i32 bias = 0;
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
                    i32 b = baseBits - 1;
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
                        main |= static_cast<u64>(n) << (64 - numSignIf);
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
                        main |= static_cast<u64>(n) << (64 - numSignIf);
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
            static_cast<Integer>(makeSign(false) | makeExponent(exp) | makeSignificand(main));

        token = makeToken(TokenType::ConstVal, temp);
    }

    bool Lexer::parseNonDecimalInteger(Token *&token, const std::string &decimalStr, i8 (*validDigits)(char),
                                       const i8 baseBits) {
        Integer v = 0;
        for(const auto decimal : decimalStr) {
            v <<= baseBits;
            v += validDigits(decimal);
        }
        token = makeToken(TokenType::ConstVal, v);
        return true;
    }

    // 如果是 . | p | P 那么isReal = true
    void Lexer::extractNumber(i8 (*validDigits)(char), const std::string &expMark, std::stringstream &ss,
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

    void Lexer::identifier(Token *&token) {
        const String name = readIdentifier();

        // if(name.isEmpty())
        //     return false;

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
            return;
        }

        token = makeToken(TokenType::Identifier, String{ name });
    }

    cial::String Lexer::readIdentifier() {
        auto ch = read(false);
        // if(!isRuneLetter(ch)) {
        //     return ""_str;
        // }
        std::stringstream stream{ "" };

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

    bool Lexer::stringConstVal(Token *&token) {
        const i32 delimiter = read(false);
        // if(delimiter != '"' && delimiter != '\'') {
        //     rewindOneChar();
        //     return false;
        // }
        return parseStringConstVal(token, static_cast<char>(delimiter)) == TemplateStringContext::State::Delimiter;
    }

    /**
     * 需要多次匹配
     * such as @"this can be embeddable like &variable;"
     */
    bool Lexer::templateStringConstVal(Token *&token) {
        // 第一次进入时初始化
        if(!tmplStrCtx.active) {
            /*i32 at =*/read(); // read '@'
            const i32 delim = read(); // read '"' or '\''
            if(delim != '"' && delim != '\'') {
                token = makeToken(TokenType::Error, "template string expected '@\"' start"_str);
                return false;
            }
            tmplStrCtx.active = true;
            tmplStrCtx.delimiter = static_cast<char>(delim);
            tmplStrCtx.stage = TemplateStringContext::Stage::Init;
        }

        switch(tmplStrCtx.stage) {
            case TemplateStringContext::Stage::Init:
                token = makeToken(TokenType::LParenthesis);
                tmplStrCtx.stage = TemplateStringContext::Stage::Text;
                return true;

            case TemplateStringContext::Stage::Text: {
                tmplStrCtx.parseState = parseStringConstVal(token, tmplStrCtx.delimiter);

                switch(tmplStrCtx.parseState) {
                    case TemplateStringContext::State::Delimiter: {
                        rewindOneChar();
                        tmplStrCtx.stage = TemplateStringContext::Stage::Terminator;
                        break;
                    }
                    case TemplateStringContext::State::Ampersand:
                    case TemplateStringContext::State::Dollar: {
                        tmplStrCtx.stage = TemplateStringContext::Stage::PlusAfterText;
                        break;
                    }
                    case TemplateStringContext::State::None: {
                        return false;
                    }
                }
                return true;
            }

            case TemplateStringContext::Stage::PlusAfterText:
                token = makeToken(TokenType::Plus);
                tmplStrCtx.stage = TemplateStringContext::Stage::ExprOpener;
                break;

            case TemplateStringContext::Stage::PlusAfterExpr:
                token = makeToken(TokenType::Plus);
                tmplStrCtx.stage = TemplateStringContext::Stage::Text;
                break;

            case TemplateStringContext::Stage::ExprOpener: {
                token = makeToken(TokenType::LParenthesis);
                tmplStrCtx.stage = TemplateStringContext::Stage::Expression;
                return true;
            }

            case TemplateStringContext::Stage::Expression: {
                // 调用常规 next 解析表达式内容
                // 注意：此处需要防止 next 再次进入 templateStringConstVal
                tmplStrCtx.active = false;
                const bool res = next(token, false);
                tmplStrCtx.active = true;

                // 检查表达式是否结束, 这里检查下一个character是否是';'或'}'
                const i32 nextCodePoint = read();
                rewindOneChar();

                if(tmplStrCtx.parseState == TemplateStringContext::State::Ampersand && nextCodePoint == ';') {
                    tmplStrCtx.stage = TemplateStringContext::Stage::ExprCloser;
                } else if(tmplStrCtx.parseState == TemplateStringContext::State::Dollar && nextCodePoint == '}') {
                    tmplStrCtx.stage = TemplateStringContext::Stage::ExprCloser;
                } else if(nextCodePoint == tmplStrCtx.delimiter) {
                    tmplStrCtx.stage = TemplateStringContext::Stage::Terminator;
                }

                return res;
            }

            case TemplateStringContext::Stage::ExprCloser: {
                read();
                token = makeToken(TokenType::RParenthesis);
                tmplStrCtx.stage = TemplateStringContext::Stage::PlusAfterExpr;
                return true;
            }

            case TemplateStringContext::Stage::Terminator: {
                read();
                token = makeToken(TokenType::RParenthesis);
                tmplStrCtx.reset(); // 清理状态
                return true;
            }
        }

        return false;
    }

    TemplateStringContext::State Lexer::parseStringConstVal(Token *&token, const char delimiter) {
        std::stringstream str{ "" };
        auto strPsState = TemplateStringContext::State::None;
        for(;;) {
            i32 ch = read(false);
            if(ch == runeEof) {
                rewindOneChar();
                break;
            }
            if(ch == '\\') {
                ch = read(false);

                // 模板模式下，\& 和 \$ 直接转义为普通字符
                if(tmplStrCtx.active && (ch == '&' || ch == '$')) {
                    str << static_cast<char>(ch);
                    continue;
                }

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

                    i32 code = 0, count = 0;
                    auto hex = getHexNum(static_cast<char>(ch));
                    while(hex != -1 && count < sizeof(i32) * 2) {
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
                    i32 code = 0;
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
                str << TemplateStringContext::unescapeBackSlash(static_cast<char>(ch));
                continue;
            }

            if(ch == delimiter) {
                strPsState = TemplateStringContext::State::Delimiter;
                break;
            }

            // 模板触发检查
            if(tmplStrCtx.active) {
                if(ch == '&') {
                    strPsState = TemplateStringContext::State::Ampersand;
                    break;
                }
                if(ch == '$') {
                    i32 nextCh = read(false);
                    if(nextCh == '{') {
                        strPsState = TemplateStringContext::State::Dollar;
                        break;
                    }
                    rewindOneChar();
                }
            }

            const auto runeType = utf8Encode(ch);
            str.write(reinterpret_cast<const char *>(runeType.data), runeType.width);
        }

        if(strPsState == TemplateStringContext::State::None) {
            token = makeToken(TokenType::Error, "string value need close"_str);
        } else {
            token = makeToken(TokenType::ConstVal, String{ str.str() });
        }

        return strPsState;
    }

    /**
     * 十六进制,字符序列
     */
    bool Lexer::octetLiteral(Token *&token) {
        std::stringstream stream{ "" };
        std::vector<u8> buf{};
        // parse an octet literal;
        // syntax is:
        // <% xx xx xx xx xx xx ... %>
        // where xx is hexadecimal 8bit(octet) binary representation.
        if(match("<%"_str)) {
            auto newSec = true;
            u8 oct = 0;

            for(;;) {
                skipComment();
                i32 ch = read(false);
                if(ch == '%') {
                    ch = read(false);
                    if(ch == '>') {
                        token = makeToken(TokenType::ConstVal, Octet{ buf.data(), static_cast<u32>(buf.size()) });
                        return true;
                    }
                    break;
                }

                const i8 n = getHexNum(static_cast<char>(ch));
                ch = static_cast<u8>(n);
                if(n != -1) {
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
                } else {
                    break;
                }
            }
        }
        token = makeToken(TokenType::Error, "unexcepted character in octet literal"_str);
        return false;
    }
} // namespace cial::Syntax