#include "Formatter.hpp"

#include <algorithm>
#include <functional>
#include "parser/Lexer.hpp"

namespace cial::format {
    namespace {
        using syntax::TokenType;
        enum class Kind { Code, LineComment, BlockComment, Opaque };
        struct Piece {
            std::string_view text;
            TokenType type;
            Kind kind;
            size_t begin;
            unsigned breaks;
            size_t match = 0;
            bool multiline = false;
        };

        bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
        bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
        unsigned lineBreaks(std::string_view s) {
            unsigned count = 0;
            for(size_t i = 0; i < s.size(); ++i)
                if(s[i] == '\n' || (s[i] == '\r' && (i + 1 == s.size() || s[i + 1] != '\n')))
                    count = std::min(2U, count + 1);
            return count;
        }
        bool fail(Common::SourceFile &file, Common::Result &result, size_t position, const std::string &message) {
            file.error(result, message, file.spanOf(position, std::min(position + 1, file.length())), "format.invalid");
            return false;
        }

        // 使用编译器词法规则，保留字符串、正则、octet 和模板的完整源码范围。
        // enablePreProcessor=false：格式化不会求值 @set 或删除未启用分支。
        class Scanner {
            std::string_view input;
            Common::SourceFile file;
            PreProcessor pp;
            syntax::Lexer lexer;
            Common::Result &result;
            bool conditionalBoundary = false;

            bool ambiguousSlash() const {
                const auto p = file.pos();
                return p < input.size() && input[p] == '/' &&
                    (p + 1 == input.size() || (input[p + 1] != '/' && input[p + 1] != '*'));
            }
            void skipSpace() {
                auto p = file.pos();
                while(p < input.size() && space(input[p]))
                    ++p;
                file.seek(p);
            }
            std::string_view directive() {
                const auto p = file.pos();
                if(p == input.size() || input[p] != '@')
                    return {};
                auto end = p + 1;
                while(end < input.size() && letter(input[end]))
                    ++end;
                return input.substr(p + 1, end - p - 1);
            }
            bool header(std::string_view name) {
                const auto start = file.pos();
                file.seek(start + name.size() + 1);
                if(name == "endif")
                    return true;
                skipSpace();
                auto p = file.pos();
                if(p == input.size() || input[p] != '(')
                    return fail(file, result, start, "preprocessor directive expects '('");
                unsigned depth = 0;
                do {
                    if(input[p] == '(')
                        ++depth;
                    else if(input[p] == ')')
                        --depth;
                    ++p;
                } while(p < input.size() && depth);
                if(depth)
                    return fail(file, result, start, "unterminated preprocessor expression");
                file.seek(p);
                return true;
            }
            bool token(syntax::Token &value) {
                syntax::Token *next = nullptr;
                const bool ok = lexer.next(next, false);
                if(next)
                    lexer.takeOverToken(value);
                if(!ok || value.type() == TokenType::Error || value.type() == TokenType::Invalid)
                    return fail(file, result, value.span.valid() ? value.span.begin : file.pos(),
                                "unable to scan source token (unterminated or invalid literal/comment)");
                return true;
            }
            bool literal(syntax::Token &value) {
                if(!token(value))
                    return false;
                while(lexer.inTemplateString()) {
                    syntax::Token part;
                    if(!token(part))
                        return false;
                    if(part.type() == TokenType::EndOfFile)
                        return fail(file, result, file.pos(), "unterminated template string");
                }
                return true;
            }
            bool conditional() {
                const auto start = file.pos();
                if(!header("if"))
                    return false;
                unsigned depth = 1;
                bool nestedBoundary = false;
                while(file.pos() < input.size()) {
                    skipSpace();
                    const auto name = directive();
                    if(name == "if" || name == "endif" || name == "set") {
                        if(!header(name))
                            return false;
                        if(name == "if")
                            ++depth;
                        else if(name == "endif") {
                            if(--depth == 0)
                                return true;
                            nestedBoundary = true;
                        }
                    } else {
                        // 不解析分支的语法或执行预处理表达式；词法不完整时拒绝改写。
                        if(nestedBoundary && ambiguousSlash())
                            return fail(file, result, file.pos(),
                                        "ambiguous '/' after conditional preprocessor region");
                        syntax::Token ignored;
                        if(!literal(ignored))
                            return false;
                        if(ignored.type() != TokenType::LineComment && ignored.type() != TokenType::BlockComment)
                            nestedBoundary = false;
                    }
                }
                return fail(file, result, start, "unterminated @if region");
            }

        public:
            Scanner(std::string_view source, Common::Result &diagnostics, const std::string &name) :
                input(source), lexer(file, pp), result(diagnostics) {
                file.setSourceName(name);
            }

            bool scan(Vec<Piece> &pieces) {
                if(!file.load(result, std::string(input)))
                    return false;
                size_t previous = input.starts_with("\xef\xbb\xbf") ? 3 : 0;
                file.seek(previous);
                while(file.pos() < input.size()) {
                    skipSpace();
                    if(file.pos() == input.size())
                        break;
                    const auto start = file.pos();
                    Kind kind = Kind::Code;
                    TokenType type = TokenType::ConstVal;
                    size_t begin = start, end;
                    const auto name = directive();
                    if(name == "if") {
                        if(!conditional())
                            return false;
                        conditionalBoundary = true;
                        kind = Kind::Opaque;
                        end = file.pos();
                    } else if(name == "set") {
                        if(!header(name))
                            return false;
                        kind = Kind::Opaque;
                        end = file.pos();
                    } else {
                        if(name == "endif")
                            return fail(file, result, start, "unexpected @endif");
                        if(conditionalBoundary && ambiguousSlash())
                            return fail(file, result, start, "ambiguous '/' after conditional preprocessor region");
                        syntax::Token value;
                        if(!literal(value))
                            return false;
                        if(value.type() == TokenType::EndOfFile)
                            break;
                        type = value.type();
                        begin = value.span.begin;
                        end = value.span.end;
                        if(input[start] == '@') {
                            begin = start;
                            end = file.pos();
                            type = TokenType::ConstVal;
                        }
                        if(type == TokenType::LineComment) {
                            kind = Kind::LineComment;
                            while(end > begin && (input[end - 1] == '\n' || input[end - 1] == '\r'))
                                --end;
                        } else if(type == TokenType::BlockComment)
                            kind = Kind::BlockComment;
                        if(kind == Kind::Code)
                            conditionalBoundary = false;
                    }
                    if(begin < previous || end <= begin || end > input.size())
                        return fail(file, result, start, "invalid source token range");
                    pieces.push_back({ input.substr(begin, end - begin), type, kind, begin,
                                       lineBreaks(input.substr(previous, begin - previous)) });
                    previous = end;
                }
                return true;
            }
        };

        bool opening(std::string_view t) { return t == "(" || t == "[" || t == "{"; }
        bool closing(std::string_view t) { return t == ")" || t == "]" || t == "}"; }
        bool control(std::string_view t) {
            return t == "if" || t == "while" || t == "for" || t == "switch" || t == "catch" || t == "with" ||
                t == "synchronized" || t == "function";
        }
        bool word(const Piece &p) {
            return p.type == TokenType::Identifier || p.type == TokenType::ConstVal || p.type == TokenType::RegExp ||
                (!p.text.empty() && (letter(p.text.front()) || static_cast<unsigned char>(p.text.front()) >= 0x80));
        }
        bool operand(const Piece &p) {
            return (word(p) && p.text != "return" && p.text != "throw" && p.text != "new" && p.text != "delete" &&
                    p.text != "typeof" && p.text != "var" && p.text != "const" && p.text != "case" &&
                    p.text != "instanceof" && p.text != "incontextof" && p.text != "in" && p.text != "else" &&
                    p.text != "do") ||
                closing(p.text) || p.text == "++" || p.text == "--";
        }
        bool unary(std::string_view t) {
            return t == "!" || t == "~" || t == "++" || t == "--" || t == "+" || t == "-" || t == "&" || t == "*" ||
                t == "#" || t == "$";
        }
        bool mergeOperators(std::string_view a, std::string_view b) {
            const std::string combined = std::string(a) + std::string(b);
#define SYMBOL_TEXT(symbol, text) std::string_view(text),
            static constexpr std::string_view operators[] = { SYMBOL_LIST(SYMBOL_TEXT) "=>", "//", "/*" };
#undef SYMBOL_TEXT
            for(const auto op : operators)
                if(op.size() > a.size() && combined.starts_with(op))
                    return true;
            return false;
        }

        class Writer {
            std::string text;
            std::string eol;
            unsigned width;
            bool lineStart = true;

        public:
            Writer(std::string newline, unsigned indent, bool bom) : eol(std::move(newline)), width(indent) {
                if(bom)
                    text = "\xef\xbb\xbf";
            }
            void newline(bool blank = false) {
                if(!lineStart) {
                    text += eol;
                    lineStart = true;
                }
                if(blank && !text.empty() && !text.ends_with(eol + eol))
                    text += eol;
            }
            void space() {
                if(!lineStart && !text.empty() && text.back() != ' ')
                    text += ' ';
            }
            void put(std::string_view raw, unsigned indent) {
                if(lineStart)
                    text.append(static_cast<size_t>(indent) * width, ' ');
                text += raw;
                lineStart = !raw.empty() && (raw.back() == '\n' || raw.back() == '\r');
            }
            std::string finish() {
                if(text.empty() || text == "\xef\xbb\xbf")
                    return text;
                newline();
                return std::move(text);
            }
        };

        struct Frame {
            std::string_view delimiter;
            bool multiline;
            bool forHeader;
            bool switchBlock;
            bool caseBody = false;
            bool caseLabel = false;
            unsigned ternaries = 0;
        };

        bool layout(Vec<Piece> &pieces, std::string &out, Common::SourceFile &file, Common::Result &result,
                    const Options &options, std::string_view input) {
            Vec<size_t> pairs;
            for(size_t i = 0; i < pieces.size(); ++i) {
                auto &p = pieces[i];
                if(p.kind != Kind::Code)
                    continue;
                if(opening(p.text)) {
                    if(pairs.size() >= 512)
                        return fail(file, result, p.begin, "delimiter nesting exceeds formatter limit (512)");
                    pairs.push_back(i);
                } else if(closing(p.text)) {
                    if(pairs.empty() ||
                       std::string_view("([{").find(pieces[pairs.back()].text) != std::string_view(")]}").find(p.text))
                        return fail(file, result, p.begin, "mismatched delimiter");
                    pieces[pairs.back()].match = i;
                    p.match = pairs.back();
                    pairs.pop_back();
                }
            }
            if(!pairs.empty())
                return fail(file, result, pieces[pairs.back()].begin, "unclosed delimiter");

            // 只看当前层的换行；子函数体新增换行不能在第二次运行时把所有外层括号变成多行。
            for(size_t i = 0; i < pieces.size(); ++i) {
                auto &p = pieces[i];
                if(p.kind != Kind::Code || !opening(p.text))
                    continue;
                p.multiline = p.text == "{";
                for(size_t j = i + 1; j <= p.match; ++j) {
                    if(pieces[j].breaks || pieces[j].kind == Kind::LineComment || pieces[j].kind == Kind::Opaque)
                        p.multiline = true;
                    if(pieces[j].kind == Kind::Code && opening(pieces[j].text))
                        j = pieces[j].match;
                }
            }
            // 轻量语句边界只用于无花括号体的缩进，不重写语法或插入分号。
            // 有效范围由匹配括号/分号及 if/else、do/while、try/catch 结构确定。
            Vec<size_t> next(pieces.size() + 1, pieces.size());
            for(size_t i = pieces.size(); i-- > 0;)
                next[i] = pieces[i].kind == Kind::LineComment || pieces[i].kind == Kind::BlockComment ? next[i + 1] : i;
            bool tooDeep = false;
            size_t budget = pieces.size() * 64 + 1024;
            std::function<size_t(size_t, unsigned)> statementEnd;
            auto afterHeader = [&](size_t index) {
                const auto open = next[index + 1];
                return open < pieces.size() && pieces[open].text == "(" ? next[pieces[open].match + 1] : pieces.size();
            };
            statementEnd = [&](size_t start, unsigned depth) -> size_t {
                if(start >= pieces.size())
                    return pieces.size();
                if(depth > 512 || budget == 0) {
                    tooDeep = true;
                    return pieces.size();
                }
                --budget;
                const auto t = pieces[start].text;
                if(pieces[start].kind == Kind::Opaque)
                    return next[start + 1];
                if(t == "{")
                    return next[pieces[start].match + 1];
                if(t == "if") {
                    auto end = statementEnd(afterHeader(start), depth + 1);
                    if(end < pieces.size() && pieces[end].text == "else")
                        end = statementEnd(next[end + 1], depth + 1);
                    return end;
                }
                if(t == "for" || t == "while" || t == "with" || t == "synchronized" || t == "switch")
                    return statementEnd(afterHeader(start), depth + 1);
                if(t == "do") {
                    auto end = statementEnd(next[start + 1], depth + 1);
                    if(end < pieces.size() && pieces[end].text == "while") {
                        end = afterHeader(end);
                        if(end < pieces.size() && pieces[end].text == ";")
                            return next[end + 1];
                    }
                    return end;
                }
                if(t == "try") {
                    auto end = statementEnd(next[start + 1], depth + 1);
                    while(end < pieces.size() && (pieces[end].text == "catch" || pieces[end].text == "finally")) {
                        const auto body = pieces[end].text == "catch" ? afterHeader(end) : next[end + 1];
                        end = statementEnd(body, depth + 1);
                    }
                    return end;
                }
                for(size_t i = start; i < pieces.size(); i = next[i + 1]) {
                    if(budget == 0) {
                        tooDeep = true;
                        return pieces.size();
                    }
                    --budget;
                    if(pieces[i].kind != Kind::Code)
                        continue;
                    if(pieces[i].text == ";")
                        return next[i + 1];
                    if(pieces[i].text == "}")
                        return i;
                    if(opening(pieces[i].text))
                        i = pieces[i].match;
                }
                return pieces.size();
            };
            Vec<int> bodyIndent(pieces.size() + 1, 0);
            Vec<bool> bodyBreak(pieces.size() + 1, false);
            auto markBody = [&](size_t start) {
                if(start >= pieces.size() || pieces[start].text == "{" || pieces[start].text == ";")
                    return;
                const auto end = statementEnd(start, 0);
                if(end > start) {
                    ++bodyIndent[start];
                    --bodyIndent[end];
                    bodyBreak[start] = true;
                }
            };
            Vec<size_t> before(pieces.size(), pieces.size());
            size_t lastSignificant = pieces.size();
            for(size_t i = 0; i < pieces.size(); ++i) {
                before[i] = lastSignificant;
                if(pieces[i].kind == Kind::Code)
                    lastSignificant = i;
            }
            for(size_t i = 0; i < pieces.size(); ++i) {
                if(pieces[i].kind != Kind::Code || (before[i] < pieces.size() && pieces[before[i]].text == "."))
                    continue;
                const auto t = pieces[i].text;
                if(t == "if" || t == "for" || t == "while" || t == "with" || t == "synchronized") {
                    const auto body = afterHeader(i);
                    markBody(body);
                    if(t == "if") {
                        const auto end = statementEnd(body, 0);
                        if(end < pieces.size() && pieces[end].text == "else") {
                            if(body < pieces.size() && pieces[body].text != "{")
                                bodyBreak[end] = true;
                            const auto other = next[end + 1];
                            if(other < pieces.size() && pieces[other].text != "if")
                                markBody(other);
                        }
                    }
                } else if(t == "do")
                    markBody(next[i + 1]);
            }
            if(tooDeep)
                return fail(file, result, 0, "statement structure exceeds formatter complexity limit");
            const auto firstNewline = input.find('\n');
            const std::string eol =
                firstNewline != std::string_view::npos && firstNewline && input[firstNewline - 1] == '\r' ? "\r\n"
                                                                                                          : "\n";
            Writer writer(eol, options.indentWidth, input.starts_with("\xef\xbb\xbf"));
            Vec<Frame> stack;
            unsigned indent = 0, topTernaries = 0;
            bool ended = false, previousUnary = false;
            const Piece *lastCode = nullptr, *previous = nullptr;
            for(size_t i = 0; i < pieces.size(); ++i) {
                auto &p = pieces[i];
                indent = static_cast<unsigned>(static_cast<int>(indent) + bodyIndent[i]);
                if(indent > 512)
                    return fail(file, result, p.begin, "indentation exceeds formatter limit (512)");
                if(bodyBreak[i])
                    writer.newline();
                const auto t = p.text;
                const auto prev = previous ? previous->text : std::string_view{};
                const auto code = lastCode ? lastCode->text : std::string_view{};
                if(p.kind == Kind::Opaque) {
                    writer.newline();
                    writer.put(t, indent);
                    writer.newline();
                    previous = &p;
                    ended = false;
                    continue;
                }
                if(p.kind == Kind::LineComment || p.kind == Kind::BlockComment) {
                    if(p.breaks || code == "{")
                        writer.newline(p.breaks > 1 && code != "{");
                    else
                        writer.space();
                    writer.put(t, indent);
                    if(p.kind == Kind::LineComment)
                        writer.newline();
                    previous = &p;
                    continue;
                }
                const bool isClose = closing(t);
                const bool isCase =
                    (t == "case" || t == "default") && code != "." && !stack.empty() && stack.back().switchBlock;
                if(isCase && stack.back().caseBody) {
                    --indent;
                    stack.back().caseBody = false;
                }
                if(isClose) {
                    const auto &frame = stack.back();
                    if(frame.caseBody)
                        --indent;
                    if(frame.multiline) {
                        --indent;
                        if(code != frame.delimiter)
                            writer.newline();
                    }
                }
                if(isCase)
                    writer.newline();
                else if(ended && t != "else")
                    writer.newline(p.breaks > 1);
                else if(code == "{" && t != "}")
                    writer.newline();
                else if(code == "}" && t != ";" && t != "," && t != ")" && t != "]" && t != "." && t != "(" &&
                        t != "else" && t != "catch" && t != "finally" && t != "while" && word(p))
                    writer.newline(p.breaks > 1);
                else if(p.breaks && !isClose && code != "{" && t != "else" && t != "catch" && t != "finally")
                    writer.newline(p.breaks > 1);

                bool prefix = unary(t) && (bodyBreak[i] || !lastCode || !operand(*lastCode));
                bool needSpace = previous != nullptr;
                if(t == ";" || t == "," || t == ")" || t == "]" || t == "." || code == ".")
                    needSpace = false;
                else if(t == "(" && !control(code))
                    needSpace = false;
                else if(t == "[" && lastCode && operand(*lastCode))
                    needSpace = false;
                else if(code == "(" || code == "[" || (code == "{" && t == "}"))
                    needSpace = false;
                else if((t == "++" || t == "--") && lastCode && operand(*lastCode))
                    needSpace = false;
                else if(previousUnary)
                    needSpace = false;
                if(t == ":") {
                    auto &ternaries = stack.empty() ? topTernaries : stack.back().ternaries;
                    needSpace = ternaries != 0;
                }
                if(t == "*" && lastCode && operand(*lastCode) && i + 1 < pieces.size() &&
                   (pieces[i + 1].text == ")" || pieces[i + 1].text == ","))
                    needSpace = false;
                if(t == "[" && code == "%")
                    needSpace = false;
                if(t == "." && lastCode && lastCode->type == TokenType::ConstVal && !code.empty() &&
                   (code.front() == '.' || (code.front() >= '0' && code.front() <= '9')))
                    needSpace = true;
                if(previous && previous->kind != Kind::Code)
                    needSpace = true;
                if(lastCode && mergeOperators(code, t))
                    needSpace = true;
                if(needSpace)
                    writer.space();
                writer.put(t, indent);
                ended = false;
                if(opening(t)) {
                    const bool multiline = p.multiline;
                    stack.push_back({ t, multiline, t == "(" && code == "for",
                                      t == "{" && lastCode && code == ")" && before[lastCode->match] < pieces.size() &&
                                          pieces[before[lastCode->match]].text == "switch" });
                    if(multiline) {
                        ++indent;
                        if(p.match != i + 1)
                            writer.newline();
                    }
                } else if(isClose)
                    stack.pop_back();
                else if(t == ";")
                    ended = stack.empty() || !stack.back().forHeader;
                else if(t == "," && !stack.empty() && stack.back().multiline)
                    writer.newline();
                else if(t == "?")
                    ++(stack.empty() ? topTernaries : stack.back().ternaries);
                else if(t == ":") {
                    auto &ternaries = stack.empty() ? topTernaries : stack.back().ternaries;
                    if(ternaries)
                        --ternaries;
                    else if(!stack.empty() && stack.back().switchBlock && stack.back().caseLabel) {
                        writer.newline();
                        ++indent;
                        stack.back().caseBody = true;
                        stack.back().caseLabel = false;
                    }
                }
                if(isCase)
                    stack.back().caseLabel = true;
                previousUnary = prefix;
                previous = lastCode = &p;
            }
            out = writer.finish();
            return true;
        }
    } // namespace

    bool formatSource(std::string_view source, std::string &output, Common::Result &result, const Options &options,
                      std::string sourceName) {
        if(options.indentWidth < 1 || options.indentWidth > 16) {
            result.error("indent width must be between 1 and 16", {}, {}, "format.options");
            return false;
        }
        Vec<Piece> pieces;
        Scanner scanner(source, result, sourceName);
        if(!scanner.scan(pieces))
            return false;
        Common::SourceFile file;
        file.setSourceName(sourceName);
        if(!file.load(result, std::string(source)))
            return false;
        std::string formatted;
        if(!layout(pieces, formatted, file, result, options, source))
            return false;
        Vec<Piece> checked;
        Scanner verify(formatted, result, sourceName);
        if(!verify.scan(checked))
            return false;
        if(pieces.size() != checked.size() ||
           !std::equal(pieces.begin(), pieces.end(), checked.begin(), [](const Piece &a, const Piece &b) {
               return a.type == b.type && a.kind == b.kind && a.text == b.text;
           }))
            return fail(file, result, 0, "formatting would change source tokens; source left unchanged");
        output = std::move(formatted);
        return true;
    }
} // namespace cial::format
