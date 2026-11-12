// Copyright (c) 2026/11/11
//
// 声明索引实现：与 tools/lsp/decl_index.py 同一覆盖与推导规则。
#include "lsp/Declarations.hpp"
#include "lsp/Server.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace cial::lsp {
    namespace {
        std::string markdownDoc(const std::string &doc) {
            std::istringstream input{ doc };
            std::string out, line;
            bool example = false, parameters = false;
            for(; std::getline(input, line);) {
                if(line.starts_with('@') && example) {
                    out += "```\n";
                    example = false;
                }
                if(line.starts_with("@param ")) {
                    if(!parameters) {
                        out += "\n\n**参数**\n\n";
                        parameters = true;
                    }
                    const auto stop = line.find(' ', 7);
                    out += "- `" + line.substr(7, stop == std::string::npos ? stop : stop - 7) + "`：";
                    if(stop != std::string::npos)
                        out += line.substr(stop + 1);
                    out += '\n';
                } else if(line.starts_with("@returns "))
                    out += "\n\n**返回**：" + line.substr(9) + '\n';
                else if(line.starts_with("@return "))
                    out += "\n\n**返回**：" + line.substr(8) + '\n';
                else if(line.starts_with("@throws "))
                    out += "\n\n**异常**：" + line.substr(8) + '\n';
                else if(line.starts_with("@example")) {
                    out += "\n\n**示例**\n\n```tjs\n";
                    example = true;
                    if(line.size() > 9)
                        out += line.substr(9) + '\n';
                } else
                    out += line + '\n';
            }
            if(example)
                out += "```\n";
            return out;
        }

        std::string signatureParams(const decl::Symbol &symbol) {
            std::string out;
            for(size_t index = 0; index < symbol.params.size(); ++index) {
                if(index != 0)
                    out += ", ";
                const auto &param = symbol.params[index];
                std::string value = param.name.empty() && !param.collapse ? "arg" + std::to_string(index) : param.name;
                if(!param.type.empty()) {
                    value += ": ";
                    value += param.type;
                }
                if(param.collapse) {
                    value += '*';
                } else if(param.optional) {
                    value += " = ";
                    value += param.defaultText.empty() ? "void" : param.defaultText;
                }
                out += value;
            }
            return out;
        }
    } // namespace

    DeclarationIndex::DeclarationIndex() = default;

    void DeclarationIndex::ensureSession() {
        if(_state != nullptr)
            return;
        _runtime = std::make_unique<Runtime>();
        _context = std::make_unique<Context>(*_runtime);
        _state = std::make_unique<vm::VMState>(*_context);
    }

    void DeclarationIndex::parseFile(const std::string &uri, const std::string &text, std::vector<Entry> &out) {
        ++_parseCount;
        if(text.size() > 2000000)
            return; // 单文件上限 2 MB，超限保守放弃
        try {
            ensureSession();
            Common::Result diagnostics;
            Common::SourceFile source;
            source.setSourceName(uri);
            source.load(diagnostics, text);
            if(diagnostics.isFailed())
                return; // 无效声明不提供接口（编码非法/BOM 位置错误等）
            const decl::Module module = decl::parseDeclarations(source, diagnostics, _state->context.pp());
            if(diagnostics.isFailed())
                return; // 解析失败的声明整文件不当作事实
            const LineIndex lines{ text };
            visit(module.symbols, {}, uri, text, lines, out);
        } catch(const std::exception &) {
            // 瞬时工具故障不缓存为空接口（与 Python 版一致：本轮跳过）
        }
    }

    void DeclarationIndex::visit(const std::vector<decl::Symbol> &symbols, const std::string &owner,
                                 const std::string &uri, const std::string &text, const LineIndex &lines,
                                 std::vector<Entry> &out) const {
        for(const auto &symbol : symbols) {
            std::string qualified = owner.empty() ? symbol.name : owner + '.' + symbol.name;
            Entry entry;
            entry.symbol = symbol;
            entry.qualified = qualified;
            entry.owner = owner;
            entry.uri = uri;
            if(symbol.span.valid())
                entry.nameRange = lines.tokenRange(symbol.span.begin, symbol.span.end);
            out.push_back(std::move(entry));
            visit(symbol.members, std::move(qualified), uri, text, lines, out);
        }
    }

    void DeclarationIndex::loadSystemFiles(const std::vector<std::filesystem::path> &files) {
        _merged = false;
        _system.clear();
        for(const auto &path : files) {
            std::error_code ec;
            const auto resolved = std::filesystem::weakly_canonical(path, ec);
            if(ec || !std::filesystem::is_regular_file(resolved, ec))
                continue;
            std::ifstream file{ resolved, std::ios::binary };
            if(!file)
                continue;
            std::string text{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
            // 磁盘声明按 UTF-8 BOM 规则读取（与 SourceFile::load 语义一致）
            if(text.starts_with("\xEF\xBB\xBF"))
                text.erase(0, 3);
            parseFile(pathToUri(resolved), text, _system);
        }
        _entries.clear();
        for(const auto &entry : _system)
            _entries[entry.qualified].push_back(entry);
    }

    bool DeclarationIndex::setFile(const std::string &uri, const std::string &utf8Text) {
        std::vector<Entry> parsed;
        parseFile(uri, utf8Text, parsed);
        if(parsed.empty())
            return false;
        removeFile(uri);
        for(const auto &entry : parsed)
            _entries[entry.qualified].push_back(entry);
        return true;
    }

    void DeclarationIndex::removeFile(const std::string &uri) {
        _merged = false;
        for(auto it = _entries.begin(); it != _entries.end();) {
            std::erase_if(it->second, [&](const Entry &entry) { return entry.uri == uri; });
            if(it->second.empty())
                it = _entries.erase(it);
            else
                ++it;
        }
    }

    void DeclarationIndex::merge(const std::vector<Entry> &projectEntries) {
        _merged = false;
        // 项目声明覆盖同名系统声明（按顶层限定名首段），与 Python 版 update() 一致。
        std::set<std::string> projectRoots;
        for(const auto &entry : projectEntries) {
            const auto dot = entry.qualified.find('.');
            projectRoots.insert(entry.qualified.substr(0, dot == std::string::npos ? entry.qualified.size() : dot));
        }
        _entries.clear();
        for(const auto &entry : projectEntries)
            _entries[entry.qualified].push_back(entry);
        for(const auto &entry : _system) {
            const auto dot = entry.qualified.find('.');
            const std::string root = entry.qualified.substr(0, dot == std::string::npos ? entry.qualified.size() : dot);
            if(projectRoots.contains(root))
                continue; // 项目声明覆盖该系统接口
            _entries[entry.qualified].push_back(entry);
        }
    }

    void DeclarationIndex::reset() {
        _merged = false;
        _entries.clear();
        for(const auto &entry : _system)
            _entries[entry.qualified].push_back(entry);
    }

    void DeclarationIndex::mergeProjectSources(const std::vector<std::pair<std::string, std::string>> &sources) {
        if(_merged && sources == _lastSources)
            return;
        std::vector<Entry> entries;
        std::set<std::string> live;
        for(const auto &[uri, text] : sources) {
            live.insert(uri);
            auto found = _projectCache.find(uri);
            if(found == _projectCache.end() || found->second.text != text) {
                CachedSource source{ text, {} };
                parseFile(uri, text, source.entries);
                found = _projectCache.insert_or_assign(uri, std::move(source)).first;
            }
            entries.insert(entries.end(), found->second.entries.begin(), found->second.entries.end());
        }
        std::erase_if(_projectCache, [&](const auto &entry) { return !live.contains(entry.first); });
        merge(entries);
        _lastSources = sources;
        _merged = true;
    }

    const DeclarationIndex::Entry *DeclarationIndex::unique(const std::string &qualified) const {
        const auto it = _entries.find(qualified);
        return it != _entries.end() && it->second.size() == 1 ? &it->second.front() : nullptr;
    }

    std::map<std::string, const DeclarationIndex::Entry *>
    DeclarationIndex::membersOf(const std::string &qualified) const {
        std::set<std::string> visited;
        return membersOfImpl(qualified, visited);
    }

    std::map<std::string, const DeclarationIndex::Entry *>
    DeclarationIndex::membersOfImpl(const std::string &qualified, std::set<std::string> &visited) const {
        std::map<std::string, const Entry *> result;
        if(visited.contains(qualified) || visited.size() >= 8)
            return result;
        visited.insert(qualified);
        const Entry *owner = unique(qualified);
        if(owner == nullptr)
            return result;
        for(const auto &base : owner->symbol.extends) {
            const std::string local = owner->owner.empty() ? base : owner->owner + '.' + base;
            result.merge(membersOfImpl(unique(local) != nullptr ? local : base, visited));
        }
        for(const auto &[name, candidates] : _entries) {
            (void)name;
            if(candidates.size() == 1 && candidates.front().owner == qualified &&
               candidates.front().symbol.name != owner->symbol.name) // 构造器经类调用，不入成员表
                result[candidates.front().symbol.name] = &candidates.front();
        }
        return result;
    }

    std::string DeclarationIndex::returnType(const Entry &entry) const {
        std::string result = entry.symbol.ret;
        if(result.find('|') != std::string::npos)
            return {}; // 联合类型不能猜测唯一接收者。
        if(result.ends_with("[]"))
            return "Array";
        if(result.empty() || result == "any" || result == "void")
            return {};
        std::string owner = entry.owner;
        while(!owner.empty()) {
            const std::string local = owner + '.' + result;
            const Entry *candidate = unique(local);
            if(candidate != nullptr &&
               (candidate->symbol.kind == decl::SymbolKind::Class ||
                candidate->symbol.kind == decl::SymbolKind::Namespace))
                return local;
            const auto dot = owner.rfind('.');
            owner = dot == std::string::npos ? std::string{} : owner.substr(0, dot);
        }
        return result;
    }

    std::string DeclarationIndex::valueType(const std::string &qualified) const {
        const Entry *entry = unique(qualified);
        if(entry == nullptr || entry->symbol.kind == decl::SymbolKind::Function ||
           entry->symbol.kind == decl::SymbolKind::StaticFunction)
            return {};
        // 类/命名空间本身没有返回类型，但限定成员引用仍由其声明定义。
        if(entry->symbol.kind == decl::SymbolKind::Class || entry->symbol.kind == decl::SymbolKind::Namespace)
            return entry->qualified;
        return returnType(*entry);
    }

    std::string DeclarationIndex::callType(const std::string &qualified) const {
        const Entry *entry = unique(qualified);
        if(entry == nullptr ||
           (entry->symbol.kind != decl::SymbolKind::Function &&
            entry->symbol.kind != decl::SymbolKind::StaticFunction && entry->symbol.kind != decl::SymbolKind::Class))
            return {};
        return returnType(*entry);
    }

    std::string DeclarationIndex::signature(const decl::Symbol &symbol) {
        switch(symbol.kind) {
            case decl::SymbolKind::Function:
            case decl::SymbolKind::StaticFunction: {
                std::string out = symbol.kind == decl::SymbolKind::StaticFunction ? "static " : "";
                out += symbol.name + '(' + signatureParams(symbol) + ')';
                if(!symbol.ret.empty())
                    out += ": " + symbol.ret;
                return out;
            }
            case decl::SymbolKind::Class: {
                std::string out = "class " + symbol.name;
                if(!symbol.extends.empty()) {
                    out += " extends ";
                    for(size_t i = 0; i < symbol.extends.size(); ++i) {
                        if(i != 0)
                            out += ", ";
                        out += symbol.extends[i];
                    }
                }
                return out;
            }
            case decl::SymbolKind::Namespace:
                return "namespace " + symbol.name;
            case decl::SymbolKind::Property: {
                std::string out = symbol.name;
                if(!symbol.ret.empty())
                    out += ": " + symbol.ret;
                std::string access;
                if(symbol.hasGetter)
                    access += "get";
                if(symbol.hasSetter)
                    access += access.empty() ? "set" : ", set";
                if(!access.empty())
                    out += " (" + access + ")";
                return out;
            }
            case decl::SymbolKind::Variable:
                break;
        }
        std::string out = symbol.constant ? "const " : "var ";
        out += symbol.name;
        if(!symbol.ret.empty())
            out += ": " + symbol.ret;
        return out;
    }

    Json DeclarationIndex::completionItem(const Entry &entry, const std::string &typedPrefix) {
        const auto &symbol = entry.symbol;
        if(!typedPrefix.empty() && symbol.name.starts_with(typedPrefix) == false)
            return Json{};
        static const std::map<decl::SymbolKind, int> kinds{
            { decl::SymbolKind::Function, 2 },  { decl::SymbolKind::StaticFunction, 2 },
            { decl::SymbolKind::Class, 7 },     { decl::SymbolKind::Namespace, 9 },
            { decl::SymbolKind::Property, 10 }, { decl::SymbolKind::Variable, 5 }
        };
        Json item = Json::object();
        item.set("label", symbol.name);
        item.set("kind", kinds.at(symbol.kind));
        item.set("detail", signature(symbol));
        if(!symbol.doc.empty()) {
            Json documentation = Json::object();
            documentation.set("kind", "markdown");
            documentation.set("value", markdownDoc(symbol.doc));
            item.set("documentation", documentation);
        }
        item.set("sortText", symbol.name);
        return item;
    }

    Json DeclarationIndex::hover(const Entry &entry) {
        std::string value = "```tjs\n" + signature(entry.symbol) + "\n```";
        if(!entry.symbol.doc.empty()) {
            value += "\n\n";
            value += markdownDoc(entry.symbol.doc);
        }
        Json contents = Json::object();
        contents.set("kind", "markdown");
        contents.set("value", value);
        Json result = Json::object();
        result.set("contents", contents);
        return result;
    }

    Json DeclarationIndex::location(const Entry &entry) {
        Json start = Json::object();
        start.set("line", entry.nameRange.start.line);
        start.set("character", entry.nameRange.start.character);
        Json end = Json::object();
        end.set("line", entry.nameRange.end.line);
        end.set("character", entry.nameRange.end.character);
        Json range = Json::object();
        range.set("start", start);
        range.set("end", end);
        Json result = Json::object();
        result.set("uri", entry.uri);
        result.set("range", range);
        return result;
    }
} // namespace cial::lsp
