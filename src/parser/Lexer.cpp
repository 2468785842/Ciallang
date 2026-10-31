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
#include <algorithm>
#include <bit>
#include <fmt/format.h>
#include <stdexcept>

#include "OperatorTrie.hpp"


#include "common/Defer.hpp"
#include "common/UTF8.hpp"

#include "types/Real.hpp"

namespace cial::syntax {
    Lexer::Lexer(SourceFile &sourceFile, PreProcessor &preProcessor) :
        _sourceFile(sourceFile), _preProcessor(preProcessor) {}

    Token *Lexer::makeErrorToken(Token *&token, const char *code, const String &message) {
        token = makeToken(TokenType::Error, message);
        token->code = code;
        return token;
    }

    // 统一按字节半开区间落位：span 为权威，location 由行索引换算
    void Lexer::patchTokenLoc(Token &token, const size_t beginByte, const size_t endByte) const {
        token.span = _sourceFile.spanOf(beginByte, endByte);
        token.location = _sourceFile.locationBySpan(token.span);
    }

    // 合成 token（模板字符串拼装的括号/加号）：零宽插入点，不指向源码词素
    void Lexer::patchSyntheticToken(Token &token) const {
        const size_t pos = _sourceFile.pos();
        patchTokenLoc(token, pos, pos);
    }

    bool Lexer::hasNext() const { return _hasNext; }

    /**
     * 回溯一个字符
     */
    void Lexer::rewindOneChar() const {
        // 最近一次读取是 EOF（未消耗字节）时无可回退
        if(_sourceFile.lastWidth() == 0)
            return;

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
        DEFER {
            _hasNext = !_sourceFile.eof();
            // 记录最近产出 token（注释不计），供 isRegexStart 判定除法 vs 正则
            if(token != nullptr && token->type() != TokenType::LineComment &&
               token->type() != TokenType::BlockComment) {
                _lastTokenType = token->type();
            }
        };

        i32 rune = read(!tmplStrCtx.active);

        if(rune == runeInvalid) {
            // 读取失败：next() 已按实际宽度推进，回退到非法序列首字节；
            // 行索引按原始字节构建，此处总是可定位
            const size_t width = std::max<size_t>(_sourceFile.lastWidth(), 1);
            const size_t pos = _sourceFile.pos();
            token = makeToken(TokenType::Invalid);
            token->code = "lex.invalid-byte";
            patchTokenLoc(*token, pos >= width ? pos - width : 0, pos);
            return false;
        }

        if(rune == runeEof) {
            const size_t end = _sourceFile.length();
            token = makeToken(TokenType::EndOfFile);
            patchTokenLoc(*token, end, end);
            return true;
        }

        rewindOneChar();

        // 模板字符串状态机：各阶段内部按字节落位；
        // Expression 阶段的 token 由递归 next() 精确定位，不得覆盖
        if(tmplStrCtx.active)
            return templateStringConstVal(token);

        // preprocessor
        if(enablePreProcessor && rune == '@') {
            const size_t start = _sourceFile.pos();
            try {
                while(!_sourceFile.eof()) {
                    if(processor()) {
                        continue;
                    }
                    break;
                }
            } catch(const std::runtime_error &error) {
                makeErrorToken(token, "lex.preprocessor", String{ error.what() });
                patchTokenLoc(*token, start, _sourceFile.pos());
                return false;
            }

            rune = read();

            if(rune == runeInvalid) {
                const size_t width = std::max<size_t>(_sourceFile.lastWidth(), 1);
                const size_t pos = _sourceFile.pos();
                token = makeToken(TokenType::Invalid);
                token->code = "lex.invalid-byte";
                patchTokenLoc(*token, pos >= width ? pos - width : 0, pos);
                return false;
            }

            if(rune == runeEof) {
                const size_t end = _sourceFile.length();
                token = makeToken(TokenType::EndOfFile);
                patchTokenLoc(*token, end, end);
                return true;
            }

            rewindOneChar();
        }

        // 本 token 扫描起点（预处理跳过后的位置）；消费后的游标为终点
        const size_t start = _sourceFile.pos();

        // identifier
        if(isRuneLetter(rune)) {
            identifier(token);
            patchTokenLoc(*token, start, _sourceFile.pos());
            return true;
        }

        if(rune < 0x80) {
            if(rune >= '0' && rune <= '9') {
                const bool r = numberConstVal(token);
                patchTokenLoc(*token, start, _sourceFile.pos());
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
                    const bool r = matchOperator(token);
                    patchTokenLoc(*token, start, _sourceFile.pos());
                    return r;
                }
                case '.': {
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
                    patchTokenLoc(*token, start, _sourceFile.pos());
                    return r;
                }
                // block comment, line comment
                case '/': {
                    read(false); // '/'
                    const u32 peek = read(false);

                    rewindOneChar();
                    rewindOneChar();

                    bool r;
                    if(peek == '/') {
                        r = lineComment(token);
                    } else if(peek == '*') {
                        r = blockComment(token);
                    } else if(isRegexStart()) {
                        r = regexConstVal(token);
                    } else {
                        r = matchOperator(token);
                    }
                    patchTokenLoc(*token, start, _sourceFile.pos());
                    return r;
                }
                case '<': {
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
                    patchTokenLoc(*token, start, _sourceFile.pos());
                    return r;
                }
                case ';': {
                    read();
                    token = makeToken(TokenType::SemiColon);
                    patchTokenLoc(*token, start, _sourceFile.pos());
                    return true;
                }
                case '\'':
                case '"': {
                    const bool r = stringConstVal(token);
                    patchTokenLoc(*token, start, _sourceFile.pos());
                    return r;
                }
                case '@': {
                    const bool r = templateStringConstVal(token);
                    // 状态机内部已落位；错误路径兜底
                    if(!token->span.valid())
                        patchTokenLoc(*token, start, _sourceFile.pos());
                    return r;
                }
                default: {
                    read();
                    makeErrorToken(token, "lex.invalid-character",
                                   String{ fmt::format("unexpected character '{}'", static_cast<char>(rune)) });
                    patchTokenLoc(*token, start, _sourceFile.pos());
                    return false;
                }
            }
        }

        read();
        token = makeToken(TokenType::Invalid);
        token->code = "lex.invalid-byte";
        patchTokenLoc(*token, start, _sourceFile.pos());
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
                    throw std::runtime_error("preprocessor directive '@set' expects '('");

                std::string expr;
                if(!readPreProcessorExpr(expr))
                    throw std::runtime_error("preprocessor '@set' expression is not closed");

                _preProcessor.onSet(String{ expr });
                return true;
            }

            // ---------- @if ----------
            if(match("if"_str)) {
                if(read() != '(')
                    throw std::runtime_error("preprocessor directive '@if' expects '('");

                std::string expr;
                if(!readPreProcessorExpr(expr))
                    throw std::runtime_error("preprocessor '@if' expression is not closed");

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
        static OperatorTrie OpTrie{
#define LIST_TO_TOKEN_TYPE_PAIR(SYMBOL, NAME) { NAME##_str, TokenType::SYMBOL },
            SYMBOL_LIST(LIST_TO_TOKEN_TYPE_PAIR)
#undef LIST_TO_TOKEN_TYPE_PAIR
        };
        // 字典分隔符 => 在本解析器中借用 GtOrEqual；参考 tjsLex.cpp 将其映射为
        // T_COMMA。内部 token 复用不表示源代码中 => 与 >= 可互换；可移植脚本使用 =>。
        OpTrie.insert("=>"_str, TokenType::GtOrEqual);

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

        makeErrorToken(token, "lex.malformed-operator", "expected a operator character"_str);
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
                const i32 ch = read(false);
                if(ch == '/' || ch == '*') {
                    const i32 next = read(false);
                    if(ch == '/' && next == '*') {
                        ++blockCount;
                    } else if(ch == '*' && next == '/') {
                        if(--blockCount == 0)
                            break;
                    } else {
                        rewindOneChar();
                    }
                }
            }
            if(blockCount != 0) {
                _incomplete = true;
                // 范围由调用方落位：[注释起点, EOF]，表达与文件末尾的关系
                makeErrorToken(token, "lex.unterminated-comment", "unterminated block comment"_str);
                return false;
            }
            token = makeToken(TokenType::BlockComment);
            return true;
        }
        return false;
    }

    // '/' 的二义性判据（除法 vs 正则字面量起始）：按前一个有效 token 判定——
    // 操作数结尾（字面量/标识符/右括号等）之后是除法，其余（运算符/关键字/行首）
    // 之后是正则（JS lexer 标准做法）。源字符回看无法区分 `x /= 4` 与 `x = /re/`
    bool Lexer::isRegexStart() const {
        switch(_lastTokenType) {
            case TokenType::ConstVal: // 含数字/字符串/true/false 字面量
            case TokenType::Identifier:
            case TokenType::RegExp:
            case TokenType::Octet:
            case TokenType::RParenthesis:
            case TokenType::RBracket:
            case TokenType::RightCurlyBrace:
            case TokenType::Increment:
            case TokenType::Decrement:
                return false; // 操作数结尾 → 除法
            case TokenType::None:
            case TokenType::LineComment:
            case TokenType::BlockComment:
                return true; // 文件头
            default:
                return true; // 运算符/关键字之后 → 正则
        }
    }

    // /pattern/flags 正则字面量：'/' 已读，读到未转义的 '/'，再读 [a-z]* flags。
    // token 文本编码为 "pattern\nflags"（'\n' 作分隔符；模式内裸换行视为未终结，
    // 保证编码无歧义），NewRegExp 处解码。TJS2 flags：g=全局、i=忽略大小写、l=忽略
    bool Lexer::regexConstVal(Token *&token) {
        std::string pattern;
        bool closed = false;
        // case '/' 的 peek+双 rewind 把 '/' 退了回来：这里先消费它
        if(read(false) != '/') {
            rewindOneChar();
            makeErrorToken(token, "lex.regexp-start", "expected regex start"_str);
            return false;
        }
        while(!_sourceFile.eof()) {
            const i32 rune = read(false);
            if(rune == runeEof || rune == runeInvalid || rune == '\n') {
                makeErrorToken(token, "lex.unterminated-regexp", "unterminated regex literal"_str);
                return false;
            }
            if(rune == '/') {
                closed = true;
                break;
            }
            if(rune == 0x5C) {
                // backslash：转义序列原样保留
                pattern.push_back(0x5C);
                const i32 esc = read(false);
                if(esc == runeEof || esc == runeInvalid) {
                    makeErrorToken(token, "lex.unterminated-regexp", "unterminated regex literal"_str);
                    return false;
                }
                if(esc < 0x80) {
                    pattern.push_back(static_cast<char>(esc));
                } else {
                    const auto encoded = utf8Encode(esc);
                    pattern.append(reinterpret_cast<const char *>(encoded.data), encoded.width);
                }
                continue;
            }
            if(rune < 0x80) {
                pattern.push_back(static_cast<char>(rune));
            } else {
                const auto encoded = utf8Encode(rune);
                pattern.append(reinterpret_cast<const char *>(encoded.data), encoded.width);
            }
        }

        if(!closed) {
            makeErrorToken(token, "lex.unterminated-regexp", "unterminated regex literal"_str);
            return false;
        }

        // flags（TJS2: g/i/l）：读标识符尾巴，未知字母忽略（与 TJS2 一致）
        std::string flags;
        while(!_sourceFile.eof()) {
            if(const u32 ch = _sourceFile[_sourceFile.pos()]; (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
                flags.push_back(static_cast<char>(ch));
                _sourceFile.seek(_sourceFile.pos() + 1);
                continue;
            }
            break;
        }

        pattern.push_back('\n');
        pattern.append(flags);
        token = makeToken(TokenType::RegExp, String{ pattern.c_str(), static_cast<u32>(pattern.size()) });
        return true;
    }

    bool Lexer::numberConstVal(Token *&token) {
        std::stringstream stream{ "" };
        auto ch = read();
        constexpr std::string_view valid = ".0123456789Ee";
        i32 shifting = 0;
        auto hasActualDigits = false;
        auto valueType = ValueType::Integer;

        while(valid.find_first_of(static_cast<char>(ch)) != std::string_view::npos) {
            if(ch == '.') {
                if(valueType == ValueType::Real) {
                    makeErrorToken(token, "lex.malformed-number", "unexpected second decimal point"_str);
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
                if(isdigit(tCh) && tCh >= '0' && tCh <= '7') {
                    // octal, 八进制——无前缀字母，peek 消费的数字本身须回退
                    // （此前不回退：010 被解析成 0）
                    rewindOneChar();
                    return parseNonDecimalNumber(token, stream, getOctNum, 3);
                }

                rewindOneChar();
            }

            // 科学计数法
            if(ch == 'e' || ch == 'E') {
                const auto flag = read(false);
                i32 sign = 1;
                if(flag == '-') {
                    sign = -1;
                } else if(flag != '+') {
                    rewindOneChar(); // 指数符号可选：1e3 合法（旧实现直接词法报错）
                }

                i32 bit = 0;
                auto num = read(false);
                if(!isdigit(num)) {
                    makeErrorToken(token, "lex.malformed-number", "scientific notation expected digits"_str);
                    return false;
                }
                while(isdigit(num)) {
                    // Beyond this bound every finite double has already
                    // overflowed/underflowed; keep consuming without int UB.
                    bit = std::min(1000000, bit * 10 + num - '0');
                    num = read(false);
                }
                // 终止符不在此回退：break 出循环后函数末尾有统一 rewindOneChar
                shifting = sign * bit;
                // 指数非零时值域必为实数（1e-12 旧实现走 Integer 路径截断为 0）
                valueType = ValueType::Real;
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
            makeErrorToken(token, "lex.malformed-number", "number value expected digits"_str);
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
                // 无科学计数法位移时不得经 double 往返:|val| > 2^53 会失真
                // (如 9223372036854775807 会回绕成 i64min)
                if(shifting != 0)
                    val = static_cast<Integer>(fixValue(static_cast<double>(val)));
                token = makeToken(TokenType::ConstVal, val);
            }
            return true;
        }

        makeErrorToken(token, "lex.malformed-number", "expected a number value"_str);
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
            makeErrorToken(token, "lex.malformed-number", "unexcepted exponent"_str);
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
        // 朴素求值：mantissa × base^(−小数位数) × 2^p指数（base = 2^baseBits）。
        // 旧实现直接组装 IEEE double 位域，前导零/负指数偏置等边界多处算错
        // （0x1p-2 得 4、0X1.8P1 得 5.3e-15）；词法冷路径，朴素算法即可
        const double base = static_cast<double>(1ull << baseBits);
        double mant = 0.0;
        int fracDigits = 0;
        int exp2 = 0;
        bool pointPassed = false;

        for(size_t i = 0; i < decimalStr.length(); ++i) {
            const char c = decimalStr[i];
            if(c == '.') {
                pointPassed = true;
            } else if(c == 'p' || c == 'P') {
                ++i;
                bool neg = false;
                if(i < decimalStr.length() && (decimalStr[i] == '+' || decimalStr[i] == '-')) {
                    neg = decimalStr[i] == '-';
                    ++i;
                }
                int e = 0;
                for(; i < decimalStr.length() && isdigit(static_cast<unsigned char>(decimalStr[i])); ++i)
                    e = std::min(1000000, e * 10 + (decimalStr[i] - '0'));
                exp2 = neg ? -e : e;
                break;
            } else {
                mant = mant * base + validDigits(c);
                if(pointPassed)
                    ++fracDigits;
            }
        }

        const double value = mant * std::pow(base, -fracDigits) * std::pow(2.0, static_cast<double>(exp2));
        token = makeToken(TokenType::ConstVal, Real(value));
    }

    bool Lexer::parseNonDecimalInteger(Token *&token, const std::string &decimalStr, i8 (*validDigits)(char),
                                       const i8 baseBits) {
        UInteger v = 0;
        for(const auto decimal : decimalStr) {
            v <<= baseBits;
            v += validDigits(decimal);
        }
        token = makeToken(TokenType::ConstVal, std::bit_cast<Integer>(v));
        return true;
    }

    // 如果是 . | p | P 那么isReal = true
    void Lexer::extractNumber(i8 (*validDigits)(char), const std::string &expMark, std::stringstream &ss,
                              bool &isReal) {
        // 小数点
        bool pointFound = false;
        // 指数
        bool expFound = false;
        // 指数符号分支已自行回退时为 true——避免与函数末尾的统一回退叠加多退一个字符
        // （曾因此吞掉终止符：0x10) 的 ')' 被消费未回退，后续解析整体错位）
        bool rewound = false;

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
                    if(valid.find_first_of(ch) == std::string_view::npos)
                        break;
                    if(ch == '+' || ch == '-') {
                        ss << ch;
                        // 指数数字（十进制）直接跟在符号后；旧实现"跳空白+回退+break"
                        // 把指数数字留在流里（ss 形如 "1p-"，指数为空，从未正确工作过）
                        ch = static_cast<char>(read(false));
                        while(isdigit(ch)) {
                            ss << ch;
                            ch = static_cast<char>(read(false));
                        }
                        rewindOneChar(); // 终止符回退（最终回退，末尾不再统一回退）
                        rewound = true;
                        break;
                    }
                    // 中途回退（循环继续），末尾退出时仍需统一回退终止符
                    rewindOneChar();
                }
            } else if(isdigit(ch)) {
                ss << ch;
            } else {
                break;
            }
            ch = static_cast<char>(read(false));
        } while(validDigits(ch) != -1 || ch == expMark[0] || ch == expMark[1] || (!expFound && ch == '.') ||
                (expFound && isdigit(ch)));
        // 循环条件以 validDigits 为权威：旧条件用固定十进制字符集（+".Pp"），
        // 十六进制字母 A-F/a-f 不在其中——0x1.07CCp14 解析到 'C' 即中断变标识符

        if(!rewound)
            rewindOneChar(); // 退出时多读的一个字符（终止符）回退

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
     *
     * 各阶段自行落位：文本段覆盖整段词素（含引号/触发符），合成 token 为
     * 零宽插入点，Expression 阶段由递归 next() 精确定位（此处不得覆盖）。
     */
    bool Lexer::templateStringConstVal(Token *&token) {
        // 本阶段起点：'@'（首次进入）或上一阶段结束处
        const size_t entryPos = _sourceFile.pos();

        // 第一次进入时初始化
        if(!tmplStrCtx.active) {
            /*i32 at =*/
            read(); // read '@'
            const i32 delim = read(); // read '"' or '\''
            if(delim != '"' && delim != '\'') {
                makeErrorToken(token, "lex.template-start", "template string expected '@\"' start"_str);
                patchTokenLoc(*token, entryPos, _sourceFile.pos());
                return false;
            }
            tmplStrCtx.active = true;
            tmplStrCtx.delimiter = static_cast<char>(delim);
            tmplStrCtx.stage = TemplateStringContext::Stage::Init;
        }

        switch(tmplStrCtx.stage) {
            case TemplateStringContext::Stage::Init:
                token = makeToken(TokenType::LParenthesis);
                patchSyntheticToken(*token);
                tmplStrCtx.stage = TemplateStringContext::Stage::Text;
                return true;

            case TemplateStringContext::Stage::Text: {
                tmplStrCtx.parseState = parseStringConstVal(token, tmplStrCtx.delimiter);
                // 文本段词素范围（含结束引号或嵌入触发符；未闭合时到 EOF）
                patchTokenLoc(*token, entryPos, _sourceFile.pos());

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
                patchSyntheticToken(*token);
                tmplStrCtx.stage = TemplateStringContext::Stage::ExprOpener;
                return true;

            case TemplateStringContext::Stage::PlusAfterExpr:
                token = makeToken(TokenType::Plus);
                patchSyntheticToken(*token);
                tmplStrCtx.stage = TemplateStringContext::Stage::Text;
                return true;

            case TemplateStringContext::Stage::ExprOpener: {
                token = makeToken(TokenType::LParenthesis);
                patchSyntheticToken(*token);
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
                patchSyntheticToken(*token);
                tmplStrCtx.stage = TemplateStringContext::Stage::PlusAfterExpr;
                return true;
            }

            case TemplateStringContext::Stage::Terminator: {
                read();
                token = makeToken(TokenType::RParenthesis);
                patchSyntheticToken(*token);
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
                    if(i32 nextCh = read(false); nextCh == '{') {
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
            _incomplete = true;
            // 范围由调用方落位：[起始引号, EOF]
            token = makeToken(TokenType::Error, "string value need close"_str);
            token->code = "lex.unterminated-string";
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
                // 注意:这里不能调 skipComment()——它会递归进入完整 next() 词法分发,
                // 把 octet 字面量中间的数字当独立 token 吞掉;空白由下方循环自跳
                i32 ch = read(false);
                if(ch == '%') {
                    ch = read(false);
                    if(ch == '>') {
                        // 奇数个 hex 字符:最后半字节还在 oct 里,收尾入 buf
                        // (旧实现丢弃,如 <% 5 %> 得到空 octet)
                        if(!newSec)
                            buf.push_back(oct);
                        token = makeToken(TokenType::ConstVal, Octet{ buf.data(), static_cast<u32>(buf.size()) });
                        return true;
                    }
                    break;
                }

                // 空白/分隔符跳过(TJS 允许 <% 5 6 7 %>、<% 5,6 %> 等写法)
                if(ch == ' ' || ch == 9 || ch == 13 || ch == 10 || ch == ',')
                    continue;

                if(const i8 n = getHexNum(static_cast<char>(ch)); n != -1) {
                    ch = static_cast<u8>(n);
                    if(newSec) {
                        oct = ch;
                        newSec = false;
                    } else {
                        oct <<= 4;
                        oct += ch;
                        buf.push_back(oct);
                        newSec = true;
                    }
                } else {
                    // 未知字符:若紧跟 %> 由上方 '%>' 分支正常收尾,否则报错
                    break;
                }
            }
        }
        makeErrorToken(token, "lex.malformed-octet", "unexcepted character in octet literal"_str);
        return false;
    }
} // namespace cial::syntax
