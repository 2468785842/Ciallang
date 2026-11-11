// Copyright (c) 2026/11/11
//
// 声明文件模型实现（doc/declarations.md）：生成器与注册目录转换。
// 输出仅由模型决定，不含版本戳；generate(parse(text)) 与 text 幂等
// （tests/declaration_test.cpp 验证）。解析入口 parseDeclarations 在
// src/parser/DeclParser.cpp（需 syntax::Parser 的词法与诊断设施）。
#include "decl/DeclModel.hpp"

#include <map>
#include <stdexcept>
#include <utility>

#include "stdlib/NativeRegister.hpp"

namespace cial::decl {
    namespace {
        void emitDoc(std::string &out, const std::string &indent, const std::string &doc) {
            if(doc.empty())
                return;
            if(doc.find('\n') == std::string::npos) {
                out += indent;
                out += "/** ";
                out += doc;
                out += " */\n";
                return;
            }
            out += indent;
            out += "/**\n";
            size_t start = 0;
            for(;;) {
                const auto nl = doc.find('\n', start);
                out += indent;
                out += " * ";
                out += doc.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
                out += '\n';
                if(nl == std::string::npos)
                    break;
                start = nl + 1;
            }
            out += indent;
            out += " */\n";
        }

        void emitParams(std::string &out, const std::vector<Param> &params) {
            out += '(';
            for(size_t i = 0; i < params.size(); ++i) {
                if(i)
                    out += ", ";
                const auto &param = params[i];
                if(param.collapse) {
                    // 命名收拢 `name: type*`；未命名收拢 `*`
                    out += param.name;
                    if(!param.type.empty())
                        out += ": " + param.type;
                    out += '*';
                } else {
                    // 注册点推导不出参数名，无名参数生成 argN 占位（模型有名时用原名）
                    out += param.name.empty() ? "arg" + std::to_string(i) : param.name;
                    if(!param.type.empty())
                        out += ": " + param.type;
                    if(param.optional && !param.defaultText.empty())
                        out += " = " + param.defaultText;
                }
            }
            out += ')';
        }

        void emitFunction(std::string &out, const std::string &indent, const Symbol &symbol) {
            emitDoc(out, indent, symbol.doc);
            out += indent;
            if(symbol.kind == SymbolKind::StaticFunction)
                out += "static ";
            out += "function " + symbol.name;
            emitParams(out, symbol.params);
            if(!symbol.ret.empty())
                out += ": " + symbol.ret;
            out += ";\n";
        }

        void emitVariable(std::string &out, const std::string &indent, const Symbol &symbol) {
            emitDoc(out, indent, symbol.doc);
            out += indent;
            out += symbol.constant ? "const " : "var ";
            out += symbol.name;
            if(!symbol.ret.empty())
                out += ": " + symbol.ret;
            out += ";\n";
        }

        void emitProperty(std::string &out, const std::string &indent, const Symbol &symbol) {
            emitDoc(out, indent, symbol.doc);
            if(!symbol.hasGetter && !symbol.hasSetter) {
                // 访问方式未指定（注册点不含读写性事实，见 doc/declarations.md §2）
                out += indent + "property " + symbol.name;
                if(!symbol.ret.empty())
                    out += ": " + symbol.ret;
                out += ";\n";
                return;
            }
            out += indent + "property " + symbol.name + " {\n";
            const std::string inner = indent + "  ";
            if(symbol.hasGetter) {
                out += inner + "getter";
                if(!symbol.ret.empty())
                    out += ": " + symbol.ret;
                out += ";\n";
            }
            if(symbol.hasSetter) {
                // setter 参数名不入模型，生成固定占位名
                out += inner + "setter(value";
                if(!symbol.ret.empty())
                    out += ": " + symbol.ret;
                out += ");\n";
            }
            out += indent + "}\n";
        }

        void emitMembers(std::string &out, const std::vector<Symbol> &members, const size_t depth) {
            const std::string indent(depth * 2, ' ');
            for(const auto &member : members) {
                switch(member.kind) {
                    case SymbolKind::Namespace:
                        emitDoc(out, indent, member.doc);
                        out += indent + "namespace " + member.name + " {\n";
                        emitMembers(out, member.members, depth + 1);
                        out += indent + "}\n";
                        break;
                    case SymbolKind::Class:
                        emitDoc(out, indent, member.doc);
                        out += indent + "class " + member.name;
                        for(size_t i = 0; i < member.extends.size(); ++i)
                            out += i == 0 ? " extends " + member.extends[i] : ", " + member.extends[i];
                        out += " {\n";
                        emitMembers(out, member.members, depth + 1);
                        out += indent + "}\n";
                        break;
                    case SymbolKind::Function:
                    case SymbolKind::StaticFunction:
                        emitFunction(out, indent, member);
                        break;
                    case SymbolKind::Property:
                        emitProperty(out, indent, member);
                        break;
                    case SymbolKind::Variable:
                        emitVariable(out, indent, member);
                        break;
                }
            }
        }

        Symbol symbolFromMeta(const ApiMeta &meta) {
            Symbol symbol;
            symbol.name = meta.name;
            symbol.doc = meta.doc;
            symbol.ret = meta.ret;
            switch(meta.kind) {
                case ApiKind::Method:
                case ApiKind::Global:
                case ApiKind::Constructor:
                    symbol.kind = SymbolKind::Function;
                    break;
                case ApiKind::Static:
                    symbol.kind = SymbolKind::StaticFunction;
                    break;
                case ApiKind::Property:
                    // 只采用显式访问方式；getter-only 登记在语言层未必只读。
                    symbol.kind = SymbolKind::Property;
                    if(meta.accessSpecified) {
                        symbol.hasGetter = meta.hasGetter;
                        symbol.hasSetter = meta.hasSetter;
                    }
                    break;
                case ApiKind::Namespace:
                    symbol.kind = SymbolKind::Namespace;
                    break;
                case ApiKind::Class:
                    symbol.kind = SymbolKind::Class;
                    break;
            }
            symbol.extends = meta.extends;
            if(meta.params) {
                symbol.params = *meta.params;
            } else if(meta.variadic) {
                // (VM*, Value, size_t, Value*) 形态：参数表未知，转未命名收拢
                Param collapse;
                collapse.collapse = true;
                symbol.params.push_back(std::move(collapse));
            } else {
                symbol.params.reserve(meta.paramTypes.size());
                for(const auto &type : meta.paramTypes) {
                    Param param;
                    param.type = type; // 空串 = 类型未知（省略注解）
                    symbol.params.push_back(std::move(param));
                }
            }
            return symbol;
        }

        // 在同级中定位/创建同名同类节点；globals 登记保证先父后子
        Symbol &ensureNode(std::vector<Symbol> &siblings, const std::string &name, const SymbolKind kind) {
            for(auto &existing : siblings)
                if(existing.name == name && existing.kind == kind)
                    return existing;
            Symbol node;
            node.kind = kind;
            node.name = name;
            siblings.push_back(std::move(node));
            return siblings.back();
        }

        Symbol &nodeForOwner(std::vector<Symbol> &siblings, const std::string &owner) {
            std::vector<Symbol> *level = &siblings;
            size_t start = 0;
            for(;;) {
                const auto dot = owner.find('.', start);
                auto &node =
                    ensureNode(*level, owner.substr(start, dot == std::string::npos ? std::string::npos : dot - start),
                               SymbolKind::Namespace);
                if(dot == std::string::npos)
                    return node;
                start = dot + 1;
                level = &node.members;
            }
        }
    } // namespace

    std::string generateDeclarations(const Module &module) {
        std::string out;
        emitMembers(out, module.symbols, 0);
        return out;
    }

    Module declarationsFromCatalog(const ApiCatalog &catalog) {
        Module module;
        // 类符号：按注册名排序保证确定性（typeMembers 为 unordered_map）；
        // 同名注册合并成员列表（登记名冲突的诊断属后续设计，见 doc/declarations.md §6）
        std::map<std::string, Symbol> classSymbols;
        for(const auto &[key, members] : catalog.typeMembers) {
            const auto nameIt = catalog.typeNames.find(key);
            if(nameIt == catalog.typeNames.end())
                continue;
            auto &symbol = classSymbols[nameIt->second];
            symbol.kind = SymbolKind::Class;
            symbol.name = nameIt->second;
            for(const auto &meta : members)
                symbol.members.push_back(symbolFromMeta(meta));
        }

        // globals 按 owner 点分路径归位，保持登记顺序
        for(const auto &meta : catalog.globals) {
            auto &parent = meta.owner.empty() ? module.symbols : nodeForOwner(module.symbols, meta.owner).members;
            switch(meta.kind) {
                case ApiKind::Namespace:
                    ensureNode(parent, meta.name, SymbolKind::Namespace);
                    break;
                case ApiKind::Class: {
                    auto &node = ensureNode(parent, meta.name, SymbolKind::Class);
                    node.doc = meta.doc;
                    node.extends = meta.extends;
                    const auto claimed = classSymbols.find(meta.name);
                    if(claimed != classSymbols.end()) {
                        node.members = std::move(claimed->second.members);
                        classSymbols.erase(claimed);
                    }
                    break;
                }
                default:
                    parent.push_back(symbolFromMeta(meta));
                    break;
            }
        }

        // 未被 globals 认领的原生类型（标准库等）按名排序前插
        size_t index = 0;
        for(auto &[name, symbol] : classSymbols) {
            (void)name;
            module.symbols.insert(module.symbols.begin() + static_cast<ptrdiff_t>(index), std::move(symbol));
            ++index;
        }
        return module;
    }
} // namespace cial::decl
