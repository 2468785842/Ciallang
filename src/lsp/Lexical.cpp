// Copyright (c) 2026/11/11
//
// 词法索引实现：tools/lsp/cial_lsp.py DefinitionIndex 的移植（不含 rename 引用收集）。
#include "lsp/Lexical.hpp"

#include <algorithm>
#include <regex>
#include <set>

#include "common/UTF16.hpp"

namespace cial::lsp {
    namespace {
        constexpr int MAX_INFER_DEPTH = 3;
        constexpr int MAX_INFER_STEPS = 20000;

        bool isWordByte(const unsigned char ch) {
            // 标识符字节：ASCII 词字符 + $_ + 高字节（UTF-8 多字节标识符按整词处理）
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
                ch == '$' || ch >= 0x80;
        }

        bool isSpaceByte(const unsigned char ch) {
            return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
        }

        // 纯语言关键字（TJS2 语义，无 API 事实）；类型名等接口一律来自声明索引。
        const std::set<std::string> &languageKeywords() {
            static const std::set<std::string> keywords = {
                "function", "var",     "const",      "class",   "property", "namespace", "static",  "getter",
                "setter",   "catch",   "try",        "finally", "with",     "return",    "if",      "else",
                "while",    "for",     "switch",     "case",    "break",    "continue",  "default", "new",
                "throw",    "this",    "super",      "global",  "void",     "true",      "false",   "null",
                "do",       "extends", "instanceof", "typeof",  "delete",   "in",        "debugger"
            };
            return keywords;
        }

        // 表达式语境判定：前置词决定 `/` 是除法还是正则（maskedSource 用）。
        bool isExpressionKeyword(std::string_view word) {
            return word == "return" || word == "throw" || word == "case" || word == "delete" || word == "typeof" ||
                word == "in";
        }

        bool isDigits(std::string_view text) {
            return !text.empty() && std::ranges::all_of(text, [](const char ch) { return ch >= '0' && ch <= '9'; });
        }

        bool isHexLiteral(std::string_view text) {
            if(text.size() < 3 || text[0] != '0' || (text[1] != 'x' && text[1] != 'X'))
                return false;
            return std::ranges::all_of(text.substr(2), [](const char ch) {
                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
            });
        }

        bool isExponentReal(std::string_view text) {
            // \d+[eE][+-]?\d+
            size_t i = 0;
            while(i < text.size() && text[i] >= '0' && text[i] <= '9')
                ++i;
            if(i == 0 || i == text.size() || (text[i] != 'e' && text[i] != 'E'))
                return false;
            ++i;
            if(i < text.size() && (text[i] == '+' || text[i] == '-'))
                ++i;
            if(i >= text.size())
                return false;
            while(i < text.size() && text[i] >= '0' && text[i] <= '9')
                ++i;
            return i == text.size();
        }

        // LSP UTF-16 列 → 行内字节偏移（按完整字符边界截断）
        size_t columnOffset(std::string_view line, const int character) {
            size_t used = 0;
            size_t i = 0;
            while(i < line.size()) {
                unsigned char lead = static_cast<unsigned char>(line[i]);
                size_t charLen = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
                charLen = std::min(charLen, line.size() - i);
                const int width = static_cast<int>(Common::utf16Length(line.substr(i, charLen)));
                if(used + width > static_cast<size_t>(std::max(character, 0)))
                    break;
                used += static_cast<size_t>(width);
                i += charLen;
            }
            return i;
        }
    } // namespace

    bool isIdentifierByte(const unsigned char ch) { return isWordByte(ch); }

    std::string maskedSource(std::string_view text) {
        std::string masked{ text };
        size_t index = 0;
        bool expression = true;
        while(index < text.size()) {
            const size_t start = index;
            const unsigned char ch = static_cast<unsigned char>(text[index]);
            if(isSpaceByte(ch)) {
                ++index;
                continue;
            }
            const auto two = text.substr(index, 2);
            if(two == "//") {
                const auto nl = text.find('\n', index);
                index = nl == std::string_view::npos ? text.size() : nl;
            } else if(two == "/*") {
                const auto end = text.find("*/", index + 2);
                index = end == std::string_view::npos ? text.size() : end + 2;
            } else if(ch == '"' || ch == '\'' || (ch == '/' && expression)) {
                const char quote = static_cast<char>(ch);
                bool inClass = false;
                ++index;
                while(index < text.size()) {
                    const char current = text[index];
                    ++index;
                    if(current == '\\')
                        index = std::min(index + 1, text.size());
                    else if(quote == '/' && current == '[')
                        inClass = true;
                    else if(quote == '/' && current == ']')
                        inClass = false;
                    else if(current == quote && !inClass)
                        break;
                }
                expression = false;
            } else {
                if(isWordByte(ch)) {
                    size_t end = index;
                    while(end < text.size() && isWordByte(static_cast<unsigned char>(text[end])))
                        ++end;
                    expression = isExpressionKeyword(text.substr(index, end - index));
                    index = end;
                } else {
                    expression = text[index] != ')' && text[index] != ']' && text[index] != '}' && text[index] != '.';
                    ++index;
                }
                continue;
            }
            for(size_t offset = start; offset < index; ++offset)
                if(masked[offset] != '\n')
                    masked[offset] = ' ';
        }
        return masked;
    }

    Position LineIndex::positionAt(const size_t offset) const {
        auto it = std::ranges::upper_bound(_lineStarts, offset);
        const size_t line = static_cast<size_t>(it - _lineStarts.begin()) - 1;
        const size_t lineStart = _lineStarts[line];
        return { static_cast<int>(line),
                 static_cast<int>(
                     Common::utf16Length(std::string_view{ _text }.substr(lineStart, offset - lineStart))) };
    }

    Range LineIndex::tokenRange(const size_t start, const size_t end) const {
        const Position begin = positionAt(start);
        return { begin,
                 { begin.line,
                   begin.character +
                       static_cast<int>(Common::utf16Length(std::string_view{ _text }.substr(start, end - start))) } };
    }

    size_t LineIndex::offsetAt(const Position position) const {
        if(position.line < 0 || static_cast<size_t>(position.line) >= _lineStarts.size())
            return std::string::npos;
        const size_t lineStart = _lineStarts[static_cast<size_t>(position.line)];
        const size_t lineEnd =
            static_cast<size_t>(position.line) + 1 < _lineStarts.size() ? _lineStarts[position.line + 1] : _text.size();
        std::string_view line = std::string_view{ _text }.substr(lineStart, lineEnd - lineStart);
        if(!line.empty() && line.back() == '\n')
            line.remove_suffix(1);
        return lineStart + columnOffset(line, position.character);
    }

    std::vector<DocumentDeclaration> documentDeclarations(const LineIndex &lines) {
        static const std::regex declaration{ R"(\b(function|var|const|class|property|namespace)\s+([\w$]+))" };
        const std::string masked = maskedSource(lines.text());
        std::vector<DocumentDeclaration> symbols;
        for(std::sregex_iterator it{ masked.begin(), masked.end(), declaration }; it != std::sregex_iterator{}; ++it) {
            const auto &match = *it;
            const size_t nameStart = static_cast<size_t>(match.position(2));
            const size_t nameEnd = nameStart + match.length(2);
            symbols.push_back({ match.str(2), match.str(1), nameStart, nameEnd, lines.tokenRange(nameStart, nameEnd) });
        }
        return symbols;
    }

    DocumentIndex::DocumentIndex(const std::string &text, std::function<std::string(const std::string &)> externalType,
                                 std::function<std::string(const std::string &)> externalReturn) :
        _text(text), _externalType(std::move(externalType)), _externalReturn(std::move(externalReturn)),
        _budget(MAX_INFER_STEPS) {
        build();
    }

    void DocumentIndex::build() {
        const std::string source = maskedSource(_text);
        if(source.find('@') != std::string::npos)
            return; // 预处理指令需要真实解析器，整体保守拒绝
        for(size_t i = 0; i < source.size();) {
            if(isSpaceByte(static_cast<unsigned char>(source[i]))) {
                ++i;
                continue;
            }
            if(isWordByte(static_cast<unsigned char>(source[i]))) {
                size_t end = i;
                while(end < source.size() && isWordByte(static_cast<unsigned char>(source[end])))
                    ++end;
                _tokens.push_back({ source.substr(i, end - i), i, end });
                i = end;
            } else {
                _tokens.push_back({ source.substr(i, 1), i, i + 1 });
                ++i;
            }
            if(_tokens.size() > 100000)
                return;
        }
        const auto &tokens = _tokens;
        {
            std::vector<size_t> stack;
            for(size_t index = 0; index < tokens.size(); ++index) {
                const std::string &value = tokens[index].value;
                if(value == "(" || value == "[" || value == "{") {
                    stack.push_back(index);
                } else if(value == ")" || value == "]" || value == "}") {
                    constexpr std::string_view opens = "([{";
                    constexpr std::string_view closes = ")]}";
                    const auto which = closes.find(value);
                    if(stack.empty() || tokens[stack.back()].value != opens.substr(which, 1))
                        return;
                    _pairs[stack.back()] = index;
                    stack.pop_back();
                }
            }
            if(!stack.empty())
                return;
        }
        for(const auto &[open, close] : _pairs)
            _reversePairs[close] = open;

        // 作用域收集：块、函数/参数、类/属性体、for/with
        std::map<size_t, int> braceScope; // '{' token → 作用域下标
        _scopes.push_back({ 0, _text.size() + 1, ScopeKind::Root, -1, {} });
        for(size_t index = 0; index < tokens.size(); ++index) {
            if(tokens[index].value == "{") {
                braceScope[index] = static_cast<int>(_scopes.size());
                _scopes.push_back({ tokens[index].start, tokens[_pairs[index]].end, ScopeKind::Block, -1, {} });
            }
        }
        std::vector<std::tuple<int, size_t, size_t>> parameters; // (scope, 参数首 token, ')' token)
        std::vector<std::tuple<size_t, int, size_t>> functions; // (名 token, scope, 体 '{' token)
        std::map<size_t, std::pair<size_t, std::optional<size_t>>> classDecls; // 名 token → (体 '{', extends 名 token)
        for(size_t index = 0; index < tokens.size(); ++index) {
            const std::string &value = tokens[index].value;
            if(value == "function" || value == "getter" || value == "setter" || value == "catch") {
                size_t cursor = index + 1;
                std::optional<size_t> name;
                if(value == "function" && identifier(cursor)) {
                    name = cursor;
                    ++cursor;
                }
                if(cursor >= tokens.size() || tokens[cursor].value != "(")
                    continue;
                const size_t closing = _pairs[cursor];
                size_t body = closing + 1;
                if(body + 1 < tokens.size() && tokens[body].value == ":")
                    body += 2; // 可选返回类型注解
                const auto brace = braceScope.find(body);
                if(brace == braceScope.end())
                    continue;
                const int scope = static_cast<int>(_scopes.size());
                _scopes.push_back({ tokens[cursor].start,
                                    _scopes[brace->second].end,
                                    value == "catch" ? ScopeKind::Catch : ScopeKind::Function,
                                    -1,
                                    {} });
                parameters.emplace_back(scope, cursor + 1, closing);
                if(name.has_value() &&
                   (index == 0 || tokens[index - 1].value == ";" || tokens[index - 1].value == "{" ||
                    tokens[index - 1].value == "}"))
                    functions.emplace_back(*name, scope, body);
            } else if((value == "class" || value == "property") && identifier(index + 1)) {
                size_t cursor = index + 2;
                std::optional<size_t> extends;
                while(cursor < tokens.size() && tokens[cursor].value != "{" && tokens[cursor].value != ";" &&
                      tokens[cursor].value != "}") {
                    if(value == "class" && tokens[cursor].value == "extends" && identifier(cursor + 1))
                        extends = cursor + 1;
                    const auto pair = _pairs.find(cursor);
                    cursor = (pair == _pairs.end() ? cursor : pair->second) + 1;
                }
                const auto brace = braceScope.find(cursor);
                if(brace != braceScope.end()) {
                    _scopes[brace->second].kind = value == "class" ? ScopeKind::Class : ScopeKind::Property;
                    if(value == "class")
                        classDecls[index + 1] = { cursor, extends };
                }
            } else if((value == "for" || value == "with") && index + 1 < tokens.size() &&
                      tokens[index + 1].value == "(") {
                const size_t closing = _pairs[index + 1];
                const size_t body = closing + 1;
                if(body >= tokens.size())
                    continue;
                size_t end = body;
                if(tokens[body].value == "{") {
                    end = _pairs[body];
                } else {
                    while(end < tokens.size() && tokens[end].value != ";") {
                        const std::string &word = tokens[end].value;
                        if(word == "if" || word == "for" || word == "while" || word == "with" || word == "try" ||
                           word == "do")
                            return; // 无大括号复合语句需要真实解析器
                        const auto pair = _pairs.find(end);
                        end = (pair == _pairs.end() ? end : pair->second) + 1;
                    }
                }
                if(end < tokens.size())
                    _scopes.push_back({ tokens[index].start,
                                        tokens[end].end,
                                        value == "for" ? ScopeKind::For : ScopeKind::With,
                                        -1,
                                        {} });
            }
        }

        // 排序 + 父链
        std::ranges::sort(_scopes, [](const Scope &a, const Scope &b) {
            return a.start != b.start ? a.start < b.start : a.end > b.end;
        });
        {
            std::vector<int> parents;
            for(int index = 0; index < static_cast<int>(_scopes.size()); ++index) {
                auto &scope = _scopes[index];
                while(!parents.empty()) {
                    const auto &top = _scopes[parents.back()];
                    if(top.start <= scope.start && scope.end <= top.end)
                        break;
                    parents.pop_back();
                }
                scope.parent = parents.empty() ? -1 : parents.back();
                parents.push_back(index);
            }
        }
        // 每个 token 的最内层作用域（批量解析用）
        {
            std::vector<int> parents;
            size_t next = 0;
            for(size_t index = 0; index < tokens.size(); ++index) {
                const size_t offset = tokens[index].start;
                while(next < _scopes.size() && _scopes[next].start <= offset)
                    parents.push_back(static_cast<int>(next++));
                while(!parents.empty()) {
                    const auto &top = _scopes[parents.back()];
                    if(top.start <= offset && offset < top.end)
                        break;
                    parents.pop_back();
                }
                if(!parents.empty())
                    _tokenScope[index] = parents.back();
            }
        }

        for(const auto &[scopeIndex, start, closing] : parameters) {
            for(const auto &[name, init] : declarators(start, closing))
                addBinding(scopeIndex, name, _scopes[scopeIndex].start).init = init;
        }
        for(const auto &[name, body, opening] : functions) {
            const int scope = _scopes[body].parent < 0 ? 0 : _scopes[body].parent;
            auto &binding = addBinding(scope, name, _scopes[scope].start);
            binding.hasBody = true;
            binding.bodyStart = opening + 1;
            binding.bodyEnd = _pairs[opening];
            // `function name(...): Ret {` 的返回类型注解（'{' 前为 ':' Ret）
            if(opening >= 2 && tokens[opening - 2].value == ":")
                binding.annotation = tokens[opening - 1].value;
        }
        for(size_t index = 0; index < tokens.size(); ++index) {
            const std::string &value = tokens[index].value;
            if((value == "var" || value == "const") && (index == 0 || tokens[index - 1].value != ".")) {
                for(const auto &[name, init] : declarators(index + 1, tokens.size())) {
                    const int scope = scopeAt(tokens[name].start);
                    if(_scopes[scope].kind == ScopeKind::Property)
                        continue; // property 体成员仍不参与绑定
                    const size_t visible = init.has_value()
                        ? tokens[std::min(init->terminator, tokens.size() - 1)].start
                        : tokens[name].end;
                    auto &binding = addBinding(scope, name, visible);
                    binding.init = init;
                }
            } else if((value == "class" || value == "property") && identifier(index + 1)) {
                const int scope = scopeAt(tokens[index].start);
                if(_scopes[scope].kind != ScopeKind::Root && _scopes[scope].kind != ScopeKind::Class)
                    continue;
                auto &binding = addBinding(scope, index + 1, _scopes[scope].start);
                binding.isClass = value == "class";
                binding.isProperty = value == "property";
                if(classDecls.contains(index + 1))
                    binding.isClass = true;
            }
        }
        _valid = true;
    }

    bool DocumentIndex::identifier(const size_t index) const {
        if(index >= _tokens.size())
            return false;
        const auto lead = static_cast<unsigned char>(_tokens[index].value[0]);
        return isalpha(lead) || lead == '_' || lead == '$' || lead >= 0x80;
    }

    size_t DocumentIndex::pairAt(const size_t tokenIndex) const {
        const auto it = _pairs.find(tokenIndex);
        return it != _pairs.end() ? it->second : std::string::npos;
    }

    std::vector<std::pair<size_t, std::optional<DocumentIndex::Binding::Init>>>
    DocumentIndex::declarators(size_t cursor, const size_t end) const {
        std::vector<std::pair<size_t, std::optional<Binding::Init>>> result;
        while(cursor < end && identifier(cursor)) {
            const size_t name = cursor;
            ++cursor;
            if(cursor + 1 < end && _tokens[cursor].value == ":")
                cursor += 2;
            const bool initialized = cursor < end && _tokens[cursor].value == "=";
            std::optional<Binding::Init> init;
            if(initialized) {
                const size_t equals = cursor;
                ++cursor;
                while(cursor < end && _tokens[cursor].value != "," && _tokens[cursor].value != ";" &&
                      _tokens[cursor].value != ")") {
                    const auto pair = _pairs.find(cursor);
                    cursor = (pair == _pairs.end() ? cursor : pair->second) + 1;
                }
                init = Binding::Init{ equals, equals + 1, cursor };
            } else if(cursor < end && _tokens[cursor].value == "*") {
                ++cursor;
            }
            result.emplace_back(name, init);
            if(cursor >= end || _tokens[cursor].value != ",")
                break;
            ++cursor;
        }
        return result;
    }

    int DocumentIndex::scopeAt(const size_t offset) const {
        for(int index = static_cast<int>(_scopes.size()) - 1; index >= 0; --index)
            if(_scopes[index].start <= offset && offset < _scopes[index].end)
                return index;
        return 0;
    }

    DocumentIndex::Binding &DocumentIndex::addBinding(const int scope, const size_t tokenIndex, const size_t visible) {
        auto &list = _scopes[scope].bindings[_tokens[tokenIndex].value];
        Binding binding;
        binding.token = tokenIndex;
        binding.visible = visible;
        // `name: Type` 的类型注解
        if(tokenIndex + 2 < _tokens.size() && _tokens[tokenIndex + 1].value == ":")
            binding.annotation = _tokens[tokenIndex + 2].value;
        list.push_back(std::move(binding));
        _bindingAt[_tokens[tokenIndex].start] = &list.back();
        return list.back();
    }

    bool DocumentIndex::spend() const {
        --_budget;
        return _budget >= 0;
    }

    DocumentIndex::ResolveResult DocumentIndex::resolveToken(const size_t cursor) const {
        const auto &token = _tokens[cursor];
        if(cursor != 0 && _tokens[cursor - 1].value == ".")
            return {};
        if(const auto declared = _bindingAt.find(token.start); declared != _bindingAt.end())
            return { declared->second, false, {} };
        if(languageKeywords().contains(token.value))
            return {};
        if(cursor + 1 < _tokens.size() && _tokens[cursor + 1].value == ":")
            return {}; // 字典键与标签不是裸引用
        const auto scoped = _tokenScope.find(cursor);
        int scope = scoped != _tokenScope.end() ? scoped->second : scopeAt(token.start);
        bool dynamic = false;
        while(scope >= 0) {
            if(_scopes[scope].kind == ScopeKind::With)
                return {};
            dynamic |= _scopes[scope].kind == ScopeKind::Class || _scopes[scope].kind == ScopeKind::Property;
            if(_scopes[scope].kind == ScopeKind::Root && dynamic)
                return {};
            const auto entries = _scopes[scope].bindings.find(token.value);
            if(entries != _scopes[scope].bindings.end()) {
                const Binding *match = nullptr;
                size_t matchCount = 0;
                for(const auto &binding : entries->second) {
                    if(_scopes[scope].kind == ScopeKind::Root || binding.visible <= token.start) {
                        match = &binding;
                        ++matchCount;
                    }
                }
                if(match != nullptr) {
                    if(_scopes[scope].kind == ScopeKind::Root && matchCount != 1)
                        return {};
                    return { match, false, {} };
                }
            }
            if(_scopes[scope].kind == ScopeKind::Function) {
                while(_scopes[scope].parent >= 0 && _scopes[_scopes[scope].parent].kind != ScopeKind::Root) {
                    scope = _scopes[scope].parent;
                    dynamic |= _scopes[scope].kind == ScopeKind::Class || _scopes[scope].kind == ScopeKind::Property ||
                        _scopes[scope].kind == ScopeKind::With;
                }
            }
            scope = _scopes[scope].parent;
        }
        return { nullptr, !dynamic, dynamic ? std::string{} : token.value };
    }

    DocumentIndex::ResolveResult DocumentIndex::resolve(const Position position) const {
        if(!_valid)
            return {};
        const int cursor = tokenCursor(position);
        if(cursor < 0 || !identifier(static_cast<size_t>(cursor)))
            return {};
        return resolveToken(static_cast<size_t>(cursor));
    }

    std::pair<size_t, size_t> DocumentIndex::tokenSpan(const size_t tokenIndex) const {
        if(tokenIndex >= _tokens.size())
            return {};
        return { _tokens[tokenIndex].start, _tokens[tokenIndex].end };
    }

    int DocumentIndex::tokenCursor(const Position position) const {
        const LineIndex lines{ _text };
        const size_t offset = lines.offsetAt(position);
        if(offset == std::string::npos)
            return -1;
        for(size_t index = 0; index < _tokens.size(); ++index)
            if(_tokens[index].start <= offset && offset < _tokens[index].end)
                return static_cast<int>(index);
        return -1;
    }

    std::optional<std::pair<int, std::string>> DocumentIndex::memberContext(const Position position) const {
        if(!_valid)
            return std::nullopt;
        const LineIndex lines{ _text };
        const size_t offset = lines.offsetAt(position);
        if(offset == std::string::npos)
            return std::nullopt;
        // 点号必须与光标同一行
        size_t lineStart = 0;
        for(int i = 0; i < position.line; ++i) {
            const auto nl = _text.find('\n', lineStart);
            if(nl == std::string::npos)
                return std::nullopt;
            lineStart = nl + 1;
        }
        std::optional<size_t> previous;
        for(size_t index = 0; index < _tokens.size(); ++index) {
            if(_tokens[index].end <= offset)
                previous = index;
        }
        if(!previous.has_value())
            return std::nullopt;
        std::string typed;
        std::optional<size_t> dot;
        if(*previous != 0 && identifier(*previous) && _tokens[*previous - 1].value == ".") {
            typed = _tokens[*previous].value;
            dot = *previous - 1;
        } else if(_tokens[*previous].value == ".") {
            dot = *previous;
        }
        if(!dot.has_value() || _tokens[*dot].start < lineStart)
            return std::nullopt;
        auto receiver = static_cast<int>(*dot) - 1;
        if(receiver >= 0 && _tokens[static_cast<size_t>(receiver)].value == ")") {
            const auto opening = _reversePairs.find(static_cast<size_t>(receiver));
            if(opening == _reversePairs.end())
                return std::nullopt;
            receiver = opening->second >= 1 ? static_cast<int>(opening->second) - 1 : -1;
        }
        // 仅支持标识符（或单次调用）接收者；`a.b.` 链式与下标结果一律 unknown。
        if(receiver < 0 || !identifier(static_cast<size_t>(receiver)))
            return std::nullopt;
        const size_t index = static_cast<size_t>(receiver);
        if(index != 0 && _tokens[index - 1].value == ".") {
            const auto nl = _text.find('\n', _tokens[index - 1].end);
            if(nl == std::string::npos || nl < _tokens[index].start)
                return std::nullopt;
        }
        return std::make_pair(receiver, typed);
    }

    std::string DocumentIndex::inferReceiverType(const int receiver) const {
        const size_t index = static_cast<size_t>(receiver);
        if(index + 1 < _tokens.size() && _tokens[index + 1].value == "(" && _pairs.contains(index + 1)) {
            const ResolveResult resolved = resolveToken(index); // foo()：函数返回值类型
            return resolved.binding != nullptr ? inferFromBinding(*resolved.binding, 0)
                                               : _externalReturn(resolved.freeName);
        }
        const ResolveResult resolved = resolveToken(index);
        if(resolved.binding != nullptr) {
            if(resolved.binding->hasBody)
                return std::string{}; // 函数值本身不是其返回值
            return inferFromBinding(*resolved.binding, 0);
        }
        return _externalType(resolved.freeName);
    }

    std::string DocumentIndex::inferFromBinding(const Binding &binding, const int depth) const {
        if(depth > MAX_INFER_DEPTH || !spend())
            return {};
        if(!binding.annotation.empty())
            return binding.annotation != "any" && binding.annotation != "void" ? binding.annotation : std::string{};
        if(binding.init.has_value())
            return inferExpression(*binding.init, depth);
        if(binding.hasBody)
            return functionReturnType(binding, depth);
        return {};
    }

    std::string DocumentIndex::inferExpression(const Binding::Init &init, const int depth) const {
        if(depth > MAX_INFER_DEPTH || !spend())
            return {};
        const auto &tokens = _tokens;
        const size_t equals = init.equals, first = init.first, terminator = init.terminator;
        if(first >= terminator || first >= tokens.size()) {
            // 表达式整体被遮蔽：字符串与正则字面量
            const size_t rawStart = equals < tokens.size() ? tokens[equals].end : _text.size();
            const size_t rawEnd = terminator < tokens.size() ? tokens[terminator].start : _text.size();
            std::string_view raw = std::string_view{ _text }.substr(rawStart, rawEnd - rawStart);
            while(!raw.empty() && isSpaceByte(static_cast<unsigned char>(raw.front())))
                raw.remove_prefix(1);
            if(!raw.empty() && (raw.front() == '"' || raw.front() == '\''))
                return "String";
            return !raw.empty() && raw.front() == '/' ? "RegExp" : std::string{};
        }
        const size_t last = terminator - 1;
        const std::string &firstToken = tokens[first].value;
        if(firstToken == "new") {
            if(first + 1 < terminator) {
                const ResolveResult resolved = resolveToken(first + 1);
                if(resolved.binding != nullptr && !resolved.binding->isClass)
                    return std::string{}; // 局部变量遮蔽声明的构造器
            }
            size_t cursor = first + 1;
            std::string parts;
            while(cursor < terminator && identifier(cursor)) {
                if(!parts.empty())
                    parts += '.';
                parts += tokens[cursor].value;
                ++cursor;
                if(cursor < terminator && tokens[cursor].value == ".") {
                    ++cursor;
                    continue;
                }
                if(cursor >= terminator || tokens[cursor].value == "(")
                    return parts; // 服务器验证 class 身份
                break;
            }
            return {};
        }
        if(firstToken == "%" && first + 1 < tokens.size() && pairAt(first + 1) == last)
            return "Dictionary";
        if(const auto pairFirst = _pairs.find(first); pairFirst != _pairs.end() && pairFirst->second == last)
            return firstToken == "[" ? "Array" : std::string{};
        if(identifier(first)) {
            size_t cursor = first + 1;
            std::vector<std::string> parts{ firstToken };
            while(cursor + 1 < terminator && tokens[cursor].value == "." && identifier(cursor + 1)) {
                parts.push_back(tokens[cursor + 1].value);
                cursor += 2;
            }
            if(parts.size() > 1 && cursor < terminator && pairAt(cursor) == last) {
                const ResolveResult resolved = resolveToken(first);
                const std::string owner = resolved.binding != nullptr ? inferFromBinding(*resolved.binding, depth + 1)
                                                                      : _externalType(resolved.freeName);
                if(!owner.empty()) {
                    std::string path = owner;
                    for(size_t i = 1; i < parts.size(); ++i)
                        path += '.' + parts[i];
                    return _externalReturn(path);
                }
                return {};
            }
            if(first + 1 < tokens.size() && tokens[first + 1].value == "(" && pairAt(first + 1) == last) {
                const ResolveResult resolved = resolveToken(first); // 函数调用
                return resolved.binding != nullptr ? inferFromBinding(*resolved.binding, depth + 1)
                                                   : _externalReturn(resolved.freeName);
            }
            if(first == last) {
                const ResolveResult resolved = resolveToken(first); // 变量拷贝链
                if(resolved.binding != nullptr && resolved.binding->hasBody)
                    return std::string{};
                return resolved.binding != nullptr ? inferFromBinding(*resolved.binding, depth + 1)
                                                   : _externalType(resolved.freeName);
            }
            return {}; // 链式/下标/双目运算：unknown
        }
        if(first == last) {
            if(isDigits(firstToken) || isHexLiteral(firstToken) || firstToken == "true" || firstToken == "false")
                return "Integer";
            if(isExponentReal(firstToken))
                return "Real";
        } else if(first + 2 == last && isDigits(firstToken) && tokens[first + 1].value == "." &&
                  isDigits(tokens[last].value)) {
            return "Real"; // 词法把 1.5 切成三个 token
        }
        return {};
    }

    std::string DocumentIndex::functionReturnType(const Binding &binding, const int depth) const {
        if(depth > MAX_INFER_DEPTH || !spend())
            return {};
        const size_t start = binding.bodyStart, stop = binding.bodyEnd;
        std::string result;
        bool seen = false;
        for(size_t index = start; index < std::min(stop, _tokens.size()); ++index) {
            if(_tokens[index].value != "return" || (index != 0 && _tokens[index - 1].value == "."))
                continue;
            if(!spend())
                return {}; // 预算耗尽：宁可 unknown，不返回部分结果
            size_t expression = index + 1;
            while(expression < stop && _tokens[expression].value != ";")
                ++expression;
            if(expression == index + 1)
                return {}; // return; 无表达式：保守放弃
            const std::string kind = inferExpression(Binding::Init{ index, index + 1, expression }, depth);
            if(kind.empty() || (seen && result != kind))
                return {};
            result = kind;
            seen = true;
        }
        return result;
    }
} // namespace cial::lsp
