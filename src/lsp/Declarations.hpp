// Copyright (c) 2026/11/11
//
// 声明索引（tools/lsp/decl_index.py 的 C++ 内化）：系统声明（构建/安装随编译器
// 提供的 declarations/ 目录）、显式 SDK 文件与工作区 .d.tjs 全部经核心声明解析器
// （decl::parseDeclarations）转为统一条目，提供补全/悬停/定义与保守类型推导。
// API 文档只来自声明文件的 /** */ 注释，服务端不内置任何接口事实（doc/lsp.md）。
#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "decl/DeclModel.hpp"
#include "lsp/Json.hpp"
#include "lsp/Lexical.hpp"
#include "runtime/Context.hpp"
#include "runtime/Runtime.hpp"
#include "vm/VMState.hpp"

namespace cial::lsp {
    class DeclarationIndex {
    public:
        struct Entry {
            decl::Symbol symbol;
            std::string qualified;
            std::string owner;
            std::string uri;
            Range nameRange;
        };

        explicit DeclarationIndex();

        // 系统声明：构建树在可执行文件旁 declarations/，安装布局为
        // <prefix>/share/ciallang/declarations；按给定路径全部解析。
        void loadSystemFiles(const std::vector<std::filesystem::path> &files);
        // 磁盘声明文件（显式配置或工作区发现），按 URI 身份覆盖。
        bool setFile(const std::string &uri, const std::string &utf8Text);
        void removeFile(const std::string &uri);
        // 解析一批项目声明来源（未保存缓冲优先于磁盘），并按同名项目声明
        // 覆盖系统声明的规则重建条目表。
        void mergeProjectSources(const std::vector<std::pair<std::string, std::string>> &sources);
        // 每轮请求前合并项目声明；与 Python 版一致的覆盖规则：
        // 同名项目声明覆盖系统声明，多义名称保守保留全部候选。
        void merge(const std::vector<Entry> &projectEntries);
        void reset();

        [[nodiscard]] const std::map<std::string, std::vector<Entry>> &entries() const noexcept { return _entries; }
        [[nodiscard]] const Entry *unique(const std::string &qualified) const;
        // 类/命名空间成员（含 extends 链，深度 ≤ 8）；构造器不入成员表。
        [[nodiscard]] std::map<std::string, const Entry *> membersOf(const std::string &qualified) const;
        [[nodiscard]] std::string returnType(const Entry &entry) const;
        // 变量值类型（函数/静态方法返回空）；调用类型（仅函数/静态/类）。
        [[nodiscard]] std::string valueType(const std::string &qualified) const;
        [[nodiscard]] std::string callType(const std::string &qualified) const;

        // 符号签名文本（悬停/补全 detail）。
        [[nodiscard]] static std::string signature(const decl::Symbol &symbol);
        // 补全项（label/kind/detail/documentation）。
        [[nodiscard]] static Json completionItem(const Entry &entry, const std::string &typedPrefix);
        // 悬停（纯文本签名 + 文档）。
        [[nodiscard]] static Json hover(const Entry &entry);
        [[nodiscard]] static Json location(const Entry &entry);
        [[nodiscard]] size_t parseCount() const noexcept { return _parseCount; }

    private:
        void parseFile(const std::string &uri, const std::string &text, std::vector<Entry> &out);
        void visit(const std::vector<decl::Symbol> &symbols, const std::string &owner, const std::string &uri,
                   const std::string &text, const LineIndex &lines, std::vector<Entry> &out) const;
        void ensureSession(); // 声明解析会话（Runtime/Context/VMState，惰性创建）
        std::map<std::string, const Entry *> membersOfImpl(const std::string &qualified,
                                                           std::set<std::string> &visited) const;

        std::unique_ptr<Runtime> _runtime;
        std::unique_ptr<Context> _context;
        std::unique_ptr<vm::VMState> _state;
        std::vector<Entry> _system; // 系统声明（不随项目覆盖规则丢弃，仅被同名项目声明遮蔽）
        std::map<std::string, std::vector<Entry>> _entries; // qualified → 条目
        struct CachedSource {
            std::string text;
            std::vector<Entry> entries;
        };
        std::map<std::string, CachedSource> _projectCache;
        std::vector<std::pair<std::string, std::string>> _lastSources;
        bool _merged{};
        size_t _parseCount{};
    };
} // namespace cial::lsp
