// Copyright (c) 2026/11/11
//
// cial-lsp 核心集 stdio 服务器（tools/lsp/cial_lsp.py 的 C++ 内化）：
// JSON-RPC 框架、全量文本同步、进程内编译诊断（后台线程 + 防抖 + 最新版本优先）、
// 补全（关键字/文档/声明成员 + 保守类型推导）、悬停、跳转定义、文档符号与
// 工作区符号。诊断与声明解析都在进程内完成，不再派生子进程。
// API 事实只来自随编译器分发的 .d.tjs 与项目声明（doc/lsp.md），服务端不内置接口。
#pragma once

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "lsp/Declarations.hpp"
#include "lsp/Json.hpp"
#include "lsp/Lexical.hpp"

namespace cial::lsp {
    class Server {
    public:
        // --declarations 指定的额外声明目录（缺省自动发现可执行文件旁 declarations/
        // 与安装布局 share/ciallang/declarations）。
        explicit Server(std::vector<std::filesystem::path> declarationDirectories = {});
        ~Server();

        int run(std::istream &in, std::ostream &out);

    private:
        struct Document {
            std::string text; // 已剥离首个 UTF-8 BOM 的正文
            int version{};
        };
        struct PendingCheck {
            int version{};
            std::string text;
        };

        // 框架与分发
        bool readMessage(std::istream &in, std::string &content);
        void sendMessage(std::ostream &out, const Json &message);
        void respond(std::ostream &out, const Json &id, Json result);
        void respondError(std::ostream &out, const Json &id, int code, const std::string &message);
        Json dispatch(const Json &message, bool &shutdown);

        // 请求处理
        Json onInitialize(const Json &params);
        Json onCompletion(const Json &params);
        Json onHover(const Json &params);
        Json onDefinition(const Json &params);
        Json onDocumentSymbol(const Json &params);
        Json onWorkspaceSymbol(const Json &params);
        void onDidOpen(const Json &params);
        void onDidChange(const Json &params);
        void onDidClose(const Json &params);
        void onDidChangeFolders(const Json &params);

        // 文档与索引辅助
        Document document(const std::string &uri) const;
        void storeDocument(const std::string &uri, const std::string &text, int version, bool publishEmpty);
        DocumentIndex *documentIndex(const std::string &uri, const std::string &text);
        const std::vector<DocumentDeclaration> &indexedDeclarations(const std::string &uri, const std::string &text);
        void refreshWorkspaceIndex();
        void onWatchedFiles(const Json &params);
        void scheduleCheck(const std::string &uri, int version, const std::string &text);
        void checkLoop();
        void publishDiagnostics(const std::string &uri, const Json &diagnostics);
        Json diagnosticsFor(const std::string &uri, const std::string &text);
        // 刷新项目声明（加锁；projectDeclarations 需在持有 _mutex 时调用）
        void refreshDeclarations();
        // 项目声明来源：显式文件 + 工作区 .d.tjs（磁盘与未保存缓冲）；需持有 _mutex。
        std::vector<std::pair<std::string, std::string>> projectDeclarationSources();
        struct WorkspaceSymbol {
            std::string name;
            std::string keyword;
            std::string uri;
            Range range;
        };
        std::vector<WorkspaceSymbol> workspaceSources();
        struct IndexedFile {
            std::filesystem::file_time_type modified;
            uintmax_t size{};
            std::string uri;
            std::string text;
            std::vector<DocumentDeclaration> symbols;
        };
        const IndexedFile *readIndexedFile(const std::filesystem::path &path,
                                           std::map<std::filesystem::path, IndexedFile> &cache);
        struct IndexedDocument {
            std::string text;
            std::unique_ptr<DocumentIndex> lexical;
            std::vector<DocumentDeclaration> symbols;
        };
        std::map<std::filesystem::path, IndexedFile> _diskIndex;
        std::map<std::filesystem::path, IndexedFile> _explicitIndex;
        std::map<std::string, IndexedDocument> _documentIndex;
        bool _workspaceDirty{ true };
        std::chrono::steady_clock::time_point _nextIndexScan{};
        size_t _indexScans{}, _fileReads{}, _lexicalBuilds{};
        // 工作区 class 成员回退：声明索引未命中类型时，在已打开文档中查找
        // `class <typeName>` 并收集类体内的声明（v1 边界：仅已打开文档）。
        struct WorkspaceMember {
            std::string name;
            std::string keyword;
            std::string uri;
            Range range;
        };
        [[nodiscard]] std::vector<WorkspaceMember> workspaceClassMembers(const std::string &typeName) const;

        mutable std::mutex _mutex; // documents/roots/declarations/显式文件 的互斥
        std::map<std::string, Document> _documents;
        std::set<std::filesystem::path> _roots;
        std::vector<std::string> _declarationFiles; // initializationOptions.declarationFiles（URI）
        std::vector<std::filesystem::path> _declarationDirectories;
        DeclarationIndex _declarations;
        bool _systemLoaded{};

        // 诊断线程（后台编译，不阻塞请求主循环）
        std::mutex _checkMutex;
        std::condition_variable _checkWake;
        std::map<std::string, PendingCheck> _pending;
        std::set<std::string> _queued;
        bool _stop{};
        std::thread _worker;
        std::mutex _sinkMutex; // stdio 帧写出的互斥（后台发布与主线程应答）
    };

    // URI ↔ 路径：识别盘符 %3A、百分号解码其余部分；非 file URI 返回空。
    std::filesystem::path uriToPath(const std::string &uri);
    std::string pathToUri(const std::filesystem::path &path);
} // namespace cial::lsp
