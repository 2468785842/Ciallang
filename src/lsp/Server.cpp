// Copyright (c) 2026/11/11
//
// cial-lsp 服务器实现。协议参考 LSP 3.17；范围与边界见 doc/lsp.md。
#include "lsp/Server.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <regex>
#include <sstream>

#include "cli/Compilation.hpp"
#include "common/UTF16.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace cial::lsp {
    namespace {
        constexpr size_t MAX_DOCUMENT_BYTES = 2000000;
        constexpr size_t MAX_WORKSPACE_BYTES = 20000000;
        constexpr size_t MAX_WORKSPACE_FILES = 2000;
        constexpr size_t MAX_WORKSPACE_DIRECTORIES = 10000;
        constexpr size_t MAX_WORKSPACE_SYMBOLS = 200;
        constexpr int CHECK_DEBOUNCE_MILLISECONDS = 150;

        const std::set<std::string> &excludedDirectories() {
            static const std::set<std::string> excluded = {
                ".git", ".idea", ".vscode", ".venv", ".cache", "build", "bin", "ext", "node_modules", "__pycache__"
            };
            return excluded;
        }

        // 纯语言关键字说明（语言语义，非 API 事实；接口文档一律来自 .d.tjs）。
        const std::map<std::string, std::string> &keywordDocs() {
            static const std::map<std::string, std::string> docs = {
                { "function", "定义函数：function name(parameters) { ... }" },
                { "var", "声明变量：var name = value;" },
                { "const", "声明带初值的变量；当前 TJS2 兼容语义仍允许后续赋值。" },
                { "class", "定义类：class Name extends Base { ... }" },
                { "property", "定义具有 getter / setter 的属性。" },
                { "namespace", ".d.tjs 命名空间声明：namespace Name { ... }" },
                { "static", ".d.tjs 类静态方法声明。" },
                { "try", "try { ... } catch(e) { ... }" },
                { "with", "with(object) { .member; }" },
                { "return", "返回函数结果。" },
                { "if", "if(condition) statement; else statement;" },
                { "while", "while(condition) statement;" },
                { "for", "for(init; condition; step) statement;" },
                { "switch", "switch(value) { case value: ...; break; default: ...; }" },
                { "new", "创建类或内置容器实例。" },
                { "throw", "抛出可由 catch 捕获的脚本异常。" },
                { "this", "当前调用上下文。" },
                { "super", "父类上下文。" },
                { "global", "全局对象。" },
                { "void", "空值。" },
                { "true", "逻辑真（整数 1）。" },
                { "false", "逻辑假（整数 0）。" },
            };
            return docs;
        }

        bool isDeclarationUri(const std::string &uri) {
            std::string lower = uri;
            std::ranges::transform(lower, lower.begin(),
                                   [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return lower.size() >= 6 && lower.substr(lower.size() - 6) == ".d.tjs";
        }

        unsigned hexValue(const char ch) {
            if(ch >= '0' && ch <= '9')
                return static_cast<unsigned>(ch - '0');
            if(ch >= 'a' && ch <= 'f')
                return static_cast<unsigned>(ch - 'a' + 10);
            if(ch >= 'A' && ch <= 'F')
                return static_cast<unsigned>(ch - 'A' + 10);
            return 16;
        }

        std::string percentDecode(std::string_view value) {
            std::string out;
            for(size_t i = 0; i < value.size(); ++i) {
                if(value[i] == '%' && i + 2 < value.size() && hexValue(value[i + 1]) < 16 &&
                   hexValue(value[i + 2]) < 16) {
                    out.push_back(static_cast<char>(hexValue(value[i + 1]) * 16 + hexValue(value[i + 2])));
                    i += 2;
                } else {
                    out.push_back(value[i]);
                }
            }
            return out;
        }

        std::string utf8Path(const std::filesystem::path &path) {
            const auto utf8 = path.generic_u8string();
            return { utf8.begin(), utf8.end() };
        }

        std::string percentEncodePath(const std::filesystem::path &path) {
            static const std::string safe = "-._~!$&'()*+,;=:@/";
            const auto bytes = utf8Path(path);
            std::string out;
            for(const unsigned char ch : bytes) {
                if(std::isalnum(ch) || safe.find(static_cast<char>(ch)) != std::string::npos)
                    out.push_back(static_cast<char>(ch));
                else {
                    char escape[4] = "%00";
                    constexpr char hex[] = "0123456789ABCDEF";
                    escape[1] = hex[ch >> 4];
                    escape[2] = hex[ch & 0xF];
                    out += escape;
                }
            }
            return out;
        }

        std::filesystem::path executablePath() {
            std::vector<char> buffer(4096, '\0');
#if defined(_WIN32)
            std::vector<wchar_t> wideBuffer(32768, L'\0');
            const auto size = GetModuleFileNameW(nullptr, wideBuffer.data(), static_cast<DWORD>(wideBuffer.size()));
            if(size == 0 || size >= wideBuffer.size())
                return {};
            return std::filesystem::path{ std::wstring{ wideBuffer.data(), size } };
#elif defined(__APPLE__)
            uint32_t size = static_cast<uint32_t>(buffer.size());
            if(_NSGetExecutablePath(buffer.data(), &size) == 0)
                return std::filesystem::path{ std::string{ buffer.data() } };
            buffer.resize(size + 1);
            _NSGetExecutablePath(buffer.data(), &size);
            return std::filesystem::path{ std::string{ buffer.data() } };
#else
            const ssize_t size = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
            if(size > 0)
                return std::filesystem::path{ std::string{ buffer.data(), static_cast<size_t>(size) } };
            return {};
#endif
        }

        // 光标处（或字节偏移起）的标识符词
        std::string wordAtOffset(const std::string &text, const size_t offset) {
            size_t begin = offset;
            while(begin > 0 && isIdentifierByte(static_cast<unsigned char>(text[begin - 1])))
                --begin;
            size_t end = offset;
            while(end < text.size() && isIdentifierByte(static_cast<unsigned char>(text[end])))
                ++end;
            return end > begin ? text.substr(begin, end - begin) : std::string{};
        }

        std::string wordAt(const std::string &text, const Position position) {
            const LineIndex lines{ text };
            const size_t offset = lines.offsetAt(position);
            return offset == std::string::npos ? std::string{} : wordAtOffset(text, offset);
        }

        // 绑定的词法类别词（悬停展示）；接口细节不在此展开。
        const char *bindingKindWord(const DocumentIndex::Binding &binding) {
            if(binding.isClass)
                return "class";
            if(binding.isProperty)
                return "property";
            if(binding.hasBody)
                return "function";
            return "var";
        }

        const char *kindWord(const std::string &keyword) {
            if(keyword == "class")
                return "class";
            if(keyword == "function")
                return "function";
            if(keyword == "property")
                return "property";
            if(keyword == "namespace")
                return "namespace";
            return "var";
        }

        // LSP 通知方法永不响应（即使客户端违规附带 id）
        bool isNotificationMethod(const std::string &method) {
            return method == "initialized" || method == "textDocument/didOpen" || method == "textDocument/didChange" ||
                method == "textDocument/didClose" || method == "workspace/didChangeWorkspaceFolders" ||
                method == "exit" || method == "$/cancelRequest";
        }
    } // namespace

    Server::Server(std::vector<std::filesystem::path> declarationDirectories) :
        _declarationDirectories(std::move(declarationDirectories)) {
        if(_declarationDirectories.empty()) {
            const auto exe = executablePath();
            if(!exe.empty()) {
                const auto directory = exe.parent_path();
                // 工具 exe 可能在构建树子目录（如 tools/lsp/），逐级祖先找 cial 产物
                auto ancestor = directory;
                for(int level = 0; level < 4; ++level) {
                    _declarationDirectories.push_back(ancestor / "declarations");
                    ancestor = ancestor.parent_path();
                }
                _declarationDirectories.push_back(directory / ".." / "share" / "ciallang" / "declarations");
            }
        }
        _worker = std::thread([this] { checkLoop(); });
    }

    Server::~Server() {
        {
            std::lock_guard lock{ _checkMutex };
            _stop = true;
        }
        _checkWake.notify_all();
        if(_worker.joinable())
            _worker.join();
    }

    std::filesystem::path uriToPath(const std::string &uri) {
        if(!uri.starts_with("file://"))
            return {};
        std::string rest = uri.substr(7);
        const auto authorityEnd = rest.find('/');
        if(rest.compare(0, authorityEnd == std::string::npos ? rest.size() : authorityEnd, "localhost") == 0 &&
           authorityEnd != std::string::npos)
            rest = rest.substr(authorityEnd);
        else if(!rest.starts_with('/'))
            return {};
        rest = percentDecode(rest);
#if defined(_WIN32)
        // URI 的 UTF-8 路径不能经本地代码页转换；盘符前的 URI 斜杠不属于原生路径。
        if(rest.size() >= 3 && rest[0] == '/' && std::isalpha(static_cast<unsigned char>(rest[1])) && rest[2] == ':')
            rest.erase(0, 1);
#endif
        const std::filesystem::path path{ std::u8string{ rest.begin(), rest.end() } };
        std::error_code ec;
        const auto resolved = std::filesystem::weakly_canonical(path, ec);
        return ec ? path : resolved;
    }

    std::string pathToUri(const std::filesystem::path &path) {
        const std::string encoded = percentEncodePath(std::filesystem::weakly_canonical(path));
        // Windows 盘符路径（D:/...）补第三个斜杠；POSIX 绝对路径自带前导 /
        return encoded.starts_with('/') ? "file://" + encoded : "file:///" + encoded;
    }

    bool Server::readMessage(std::istream &in, std::string &content) {
        std::map<std::string, std::string> headers;
        std::string line;
        while(std::getline(in, line)) {
            if(!line.empty() && line.back() == '\r')
                line.pop_back();
            if(line.empty())
                break;
            const auto colon = line.find(':');
            if(colon != std::string::npos)
                headers[line.substr(0, colon)] = line.substr(colon + 1);
        }
        if(in.eof() && headers.empty())
            return false;
        const auto length = headers.find("Content-Length");
        if(length == headers.end())
            return false;
        const auto size = std::stoull(length->second);
        content.resize(size);
        in.read(content.data(), static_cast<std::streamsize>(size));
        return static_cast<size_t>(in.gcount()) == size;
    }

    void Server::sendMessage(std::ostream &out, const Json &message) {
        const std::string body = message.dump();
        std::lock_guard lock{ _sinkMutex };
        out << "Content-Length: " << body.size() << "\r\n\r\n" << body;
        out.flush();
    }

    void Server::respond(std::ostream &out, const Json &id, Json result) {
        Json message = Json::object();
        message.set("jsonrpc", "2.0");
        message.set("id", id);
        message.set("result", std::move(result));
        sendMessage(out, message);
    }

    void Server::respondError(std::ostream &out, const Json &id, const int code, const std::string &message) {
        Json error = Json::object();
        error.set("code", code);
        error.set("message", message);
        Json response = Json::object();
        response.set("jsonrpc", "2.0");
        response.set("id", id);
        response.set("error", error);
        sendMessage(out, response);
    }

    int Server::run(std::istream &in, std::ostream &out) {
        bool shutdown = false;
        while(!shutdown) {
            std::string content;
            if(!readMessage(in, content))
                break;
            std::string error;
            const Json message = Json::parse(content, &error);
            if(message.isNull()) {
                respondError(out, Json{ nullptr }, -32700, "parse error: " + error);
                continue;
            }
            const bool isRequest = message.find("id") != nullptr && !message.find("id")->isNull() &&
                !isNotificationMethod(message.stringAt("method"));
            try {
                const Json response = dispatch(message, shutdown);
                if(isRequest)
                    respond(out, *message.find("id"), response);
            } catch(const std::exception &failure) {
                if(isRequest)
                    respondError(out, *message.find("id"), -32603, failure.what());
            }
            if(message.stringAt("method") == "exit")
                break;
        }
        return shutdown ? 0 : 1;
    }

    Json Server::dispatch(const Json &message, bool &shutdown) {
        const std::string method = message.stringAt("method");
        const Json *params = message.find("params");
        static const Json empty = Json::object();
        if(params == nullptr)
            params = &empty;
        if(method == "initialize")
            return onInitialize(*params);
        if(method == "shutdown") {
            shutdown = true;
            return Json{ nullptr };
        }
        if(method == "textDocument/didOpen") {
            onDidOpen(*params);
            return Json{};
        }
        if(method == "textDocument/didChange") {
            onDidChange(*params);
            return Json{};
        }
        if(method == "textDocument/didClose") {
            onDidClose(*params);
            return Json{};
        }
        if(method == "workspace/didChangeWorkspaceFolders") {
            onDidChangeFolders(*params);
            return Json{};
        }
        if(method == "textDocument/completion")
            return onCompletion(*params);
        if(method == "textDocument/hover")
            return onHover(*params);
        if(method == "textDocument/definition")
            return onDefinition(*params);
        if(method == "textDocument/documentSymbol")
            return onDocumentSymbol(*params);
        if(method == "workspace/symbol")
            return onWorkspaceSymbol(*params);
        if(method.rfind("textDocument/", 0) == 0 || method.rfind("workspace/", 0) == 0)
            throw std::runtime_error("method not supported yet: " + method);
        return Json{}; // 未知通知忽略；未知请求由 run() 以 -32601 响应
    }

    Json Server::onInitialize(const Json &params) {
        if(const auto *folders = params.find("workspaceFolders"); folders != nullptr && folders->isArray())
            for(const auto &folder : folders->asArray()) {
                const auto path = uriToPath(folder.stringAt("uri"));
                if(!path.empty())
                    _roots.insert(path);
            }
        if(const auto *options = params.find("initializationOptions"); options != nullptr && options->isObject()) {
            if(const auto *files = options->find("declarationFiles"); files != nullptr && files->isArray()) {
                for(const auto &file : files->asArray())
                    if(file.isString())
                        _declarationFiles.push_back(file.asString());
            }
        }
        std::vector<std::filesystem::path> systemFiles;
        {
            std::lock_guard lock{ _mutex };
            if(!_systemLoaded) {
                _systemLoaded = true;
                for(const auto &directory : _declarationDirectories) {
                    std::error_code ec;
                    if(!std::filesystem::is_directory(directory, ec))
                        continue;
                    for(std::filesystem::directory_iterator it{ directory, ec }, end; !ec && it != end;
                        it.increment(ec)) {
                        const auto name = utf8Path(it->path().filename());
                        std::string lower = name;
                        std::ranges::transform(lower, lower.begin(), [](const unsigned char ch) {
                            return static_cast<char>(std::tolower(ch));
                        });
                        if(lower.size() >= 6 && lower.substr(lower.size() - 6) == ".d.tjs")
                            systemFiles.push_back(it->path());
                    }
                }
                std::ranges::sort(systemFiles);
                _declarations.loadSystemFiles(systemFiles);
            }
        }
        Json capabilities = Json::object();
        capabilities.set("positionEncoding", "utf-16");
        Json sync = Json::object();
        sync.set("openClose", true);
        sync.set("change", 1); // 全量文本同步
        capabilities.set("textDocumentSync", sync);
        Json completion = Json::object();
        Json triggers = Json::array();
        triggers.push(".");
        completion.set("triggerCharacters", triggers);
        capabilities.set("completionProvider", completion);
        capabilities.set("hoverProvider", true);
        capabilities.set("definitionProvider", true);
        capabilities.set("documentSymbolProvider", true);
        capabilities.set("workspaceSymbolProvider", true);
        Json result = Json::object();
        result.set("capabilities", capabilities);
        return result;
    }

    Server::Document Server::document(const std::string &uri) const {
        std::lock_guard lock{ _mutex };
        const auto it = _documents.find(uri);
        return it != _documents.end() ? it->second : Document{};
    }

    void Server::storeDocument(const std::string &uri, const std::string &text, const int version,
                               const bool publishEmpty) {
        {
            std::lock_guard lock{ _mutex };
            _documents[uri] = Document{ text, version };
        }
        if(publishEmpty)
            publishDiagnostics(uri, Json::array());
        scheduleCheck(uri, version, text);
    }

    void Server::onDidOpen(const Json &params) {
        const Json *textDocument = params.find("textDocument");
        if(textDocument == nullptr)
            return;
        std::string text = textDocument->stringAt("text");
        if(text.starts_with("\xEF\xBB\xBF"))
            text.erase(0, 3); // 与工作区扫描一致的 BOM 规则
        storeDocument(textDocument->stringAt("uri"), text, textDocument->intAt("version"), false);
    }

    void Server::onDidChange(const Json &params) {
        const Json *textDocument = params.find("textDocument");
        const Json *changes = params.find("contentChanges");
        if(textDocument == nullptr || changes == nullptr || !changes->isArray() || changes->asArray().empty())
            return;
        const Json &last = changes->asArray().back();
        std::string text = last.isString() ? last.asString() : last.stringAt("text");
        if(text.starts_with("\xEF\xBB\xBF"))
            text.erase(0, 3);
        storeDocument(textDocument->stringAt("uri"), text, textDocument->intAt("version"), false);
    }

    void Server::onDidClose(const Json &params) {
        const Json *textDocument = params.find("textDocument");
        if(textDocument == nullptr)
            return;
        const std::string uri = textDocument->stringAt("uri");
        {
            std::lock_guard lock{ _mutex };
            _documents.erase(uri);
        }
        {
            std::lock_guard lock{ _checkMutex };
            _pending.erase(uri);
        }
        publishDiagnostics(uri, Json::array());
    }

    void Server::onDidChangeFolders(const Json &params) {
        const Json *event = params.find("event");
        if(event == nullptr)
            return;
        std::lock_guard lock{ _mutex };
        if(const auto *added = event->find("added"); added != nullptr && added->isArray())
            for(const auto &folder : added->asArray()) {
                const auto path = uriToPath(folder.stringAt("uri"));
                if(!path.empty())
                    _roots.insert(path);
            }
        if(const auto *removed = event->find("removed"); removed != nullptr && removed->isArray())
            for(const auto &folder : removed->asArray()) {
                const auto path = uriToPath(folder.stringAt("uri"));
                if(!path.empty())
                    _roots.erase(path);
            }
    }

    void Server::scheduleCheck(const std::string &uri, const int version, const std::string &text) {
        {
            std::lock_guard lock{ _checkMutex };
            _pending[uri] = PendingCheck{ version, text }; // 只保留最新版本
        }
        _checkWake.notify_all();
    }

    void Server::checkLoop() {
        for(;;) {
            std::map<std::string, PendingCheck> batch;
            {
                std::unique_lock lock{ _checkMutex };
                _checkWake.wait_for(lock, std::chrono::milliseconds(CHECK_DEBOUNCE_MILLISECONDS),
                                    [&] { return _stop || !_pending.empty(); });
                if(_stop)
                    return;
                batch.swap(_pending);
            }
            for(const auto &[uri, pending] : batch) {
                const Json diagnostics = diagnosticsFor(uri, pending.text);
                Json notification = Json::object();
                notification.set("jsonrpc", "2.0");
                notification.set("method", "textDocument/publishDiagnostics");
                Json params = Json::object();
                params.set("uri", uri);
                params.set("version", pending.version);
                params.set("diagnostics", diagnostics);
                notification.set("params", params);
                sendMessage(std::cout, notification);
            }
        }
    }

    void Server::publishDiagnostics(const std::string &uri, const Json &diagnostics) {
        Json notification = Json::object();
        notification.set("jsonrpc", "2.0");
        notification.set("method", "textDocument/publishDiagnostics");
        Json params = Json::object();
        params.set("uri", uri);
        params.set("diagnostics", diagnostics);
        notification.set("params", params);
        sendMessage(std::cout, notification);
    }

    Json Server::diagnosticsFor(const std::string &uri, const std::string &text) {
        Common::Result diagnostics;
        try {
            Runtime runtime;
            Context context{ runtime };
            vm::VMState state{ context };
            Common::SourceFile source;
            source.setSourceName(uri);
            source.load(diagnostics, text);
            if(diagnostics.isFailed()) {
                // 编码非法等读取失败仍按带位置诊断发布（SourceFile 已定位）
            } else if(isDeclarationUri(uri)) {
                decl::parseDeclarations(source, diagnostics, state.context.pp());
            } else {
                cli::compile(diagnostics, state.context, source);
            }
        } catch(const std::exception &failure) {
            diagnostics.error(failure.what());
        }
        const LineIndex lines{ text };
        Json array = Json::array();
        for(const auto &message : diagnostics.messages()) {
            Json item = Json::object();
            const auto &span = message.span();
            const Range range = span.valid() ? lines.tokenRange(span.begin, span.end) : Range{ { 0, 0 }, { 0, 0 } };
            Json start = Json::object();
            start.set("line", range.start.line);
            start.set("character", range.start.character);
            Json end = Json::object();
            end.set("line", range.end.line);
            end.set("character", range.end.character);
            Json rangeJson = Json::object();
            rangeJson.set("start", start);
            rangeJson.set("end", end);
            item.set("range", rangeJson);
            item.set("severity",
                     message.type() == Common::ResultMessage::Types::error         ? 1
                         : message.type() == Common::ResultMessage::Types::warning ? 2
                                                                                   : 3);
            item.set("message", message.message());
            if(!message.code().empty())
                item.set("code", message.code());
            item.set("source", "cial");
            array.push(item);
        }
        return array;
    }

    std::unique_ptr<DocumentIndex> Server::documentIndex(const std::string &text) const {
        return std::make_unique<DocumentIndex>(
            text, [this](const std::string &name) { return _declarations.valueType(name); },
            [this](const std::string &path) { return _declarations.callType(path); });
    }

    std::vector<Server::WorkspaceMember> Server::workspaceClassMembers(const std::string &typeName) const {
        std::vector<WorkspaceMember> members;
        if(typeName.empty())
            return members;
        std::vector<std::pair<std::string, std::string>> docs; // (uri, text)
        {
            std::lock_guard lock{ _mutex };
            for(const auto &[uri, doc] : _documents)
                docs.emplace_back(uri, doc.text);
        }
        const std::regex classHeader{ "class\\s+" + typeName + "\\b" };
        for(const auto &[uri, text] : docs) {
            const std::string masked = maskedSource(text);
            std::smatch match;
            if(!std::regex_search(masked, match, classHeader))
                continue;
            const auto brace = masked.find('{', match.position());
            if(brace == std::string::npos)
                continue;
            int depth = 0;
            size_t bodyStart = brace, bodyEnd = brace;
            for(size_t i = brace; i < masked.size(); ++i) {
                if(masked[i] == '{') {
                    if(depth == 0)
                        bodyStart = i;
                    ++depth;
                } else if(masked[i] == '}') {
                    --depth;
                    if(depth == 0) {
                        bodyEnd = i;
                        break;
                    }
                }
            }
            if(depth != 0)
                continue; // 括号不平衡：编辑中保守放弃
            const LineIndex lines{ text };
            for(const auto &declaration : documentDeclarations(lines)) {
                if(declaration.keyword == "class" && declaration.name == typeName)
                    continue;
                if(declaration.nameStart <= bodyStart || declaration.nameStart >= bodyEnd)
                    continue; // 仅类体成员
                members.push_back({ declaration.name, declaration.keyword, uri, declaration.range });
            }
        }
        return members;
    }

    void Server::refreshDeclarations() {
        std::lock_guard lock{ _mutex };
        _declarations.mergeProjectSources(projectDeclarationSources());
    }

    std::vector<std::pair<std::string, std::string>> Server::projectDeclarationSources() const {
        // 调用方须已持有 _mutex（主循环线程）；按 64 文件 / 2 MB / 共享 20 MB 预算读取。
        constexpr size_t MAX_DECLARATION_FILES = 64;
        std::vector<std::pair<std::string, std::string>> sources; // (uri, text)
        std::set<std::filesystem::path> seen;
        for(const auto &[uri, doc] : _documents)
            if(isDeclarationUri(uri)) {
                sources.emplace_back(uri, doc.text);
                if(const auto path = uriToPath(uri); !path.empty())
                    seen.insert(path);
            }
        size_t remaining = MAX_WORKSPACE_BYTES;
        for(const auto &file : _declarationFiles) {
            if(sources.size() >= MAX_DECLARATION_FILES || remaining <= MAX_DOCUMENT_BYTES)
                break;
            const auto path = uriToPath(file);
            if(path.empty() || seen.contains(path))
                continue;
            std::error_code ec;
            if(!std::filesystem::is_regular_file(path, ec) ||
               std::filesystem::file_size(path, ec) > std::min(remaining, MAX_DOCUMENT_BYTES))
                continue;
            std::ifstream input{ path, std::ios::binary };
            if(!input)
                continue;
            std::string text{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
            if(text.starts_with("\xEF\xBB\xBF"))
                text.erase(0, 3);
            sources.emplace_back(file, text);
            seen.insert(path);
            remaining -= text.size();
        }
        for(const auto &root : _roots) {
            if(sources.size() >= MAX_DECLARATION_FILES || remaining <= MAX_DOCUMENT_BYTES)
                break;
            std::error_code ec;
            for(std::filesystem::recursive_directory_iterator
                    it{ root, std::filesystem::directory_options::skip_permission_denied, ec },
                end;
                !ec && it != end && sources.size() < MAX_DECLARATION_FILES; it.increment(ec)) {
                if(it->is_directory(ec))
                    continue;
                const auto name = utf8Path(it->path().filename());
                std::string lower = name;
                std::ranges::transform(lower, lower.begin(),
                                       [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if(lower.size() >= 6 && lower.substr(lower.size() - 6) == ".d.tjs" && !seen.contains(it->path())) {
                    std::ifstream input{ it->path(), std::ios::binary };
                    if(!input)
                        continue;
                    std::string text{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
                    if(text.size() > std::min(remaining, MAX_DOCUMENT_BYTES))
                        continue;
                    if(text.starts_with("\xEF\xBB\xBF"))
                        text.erase(0, 3);
                    sources.emplace_back(pathToUri(it->path()), text);
                    seen.insert(it->path());
                    remaining -= text.size();
                }
            }
            if(ec)
                break;
        }
        return sources;
    }

    Json Server::onCompletion(const Json &params) {
        const Json *textDocument = params.find("textDocument");
        const Json *position = params.find("position");
        if(textDocument == nullptr || position == nullptr)
            return Json::array();
        const std::string uri = textDocument->stringAt("uri");
        const Document doc = document(uri);
        if(doc.text.empty())
            return Json::array();
        refreshDeclarations();
        const Position at{ position->intAt("line"), position->intAt("character") };
        LineIndex lines{ doc.text };
        auto index = documentIndex(doc.text);
        Json items = Json::array();
        std::set<std::string> seen;
        // 成员补全：receiver. 形态走声明推导；无接收者回落关键字 + 文档声明 + API 顶层
        if(const auto member = index->memberContext(at); member.has_value()) {
            const std::string type = index->inferReceiverType(member->first);
            if(!type.empty()) {
                const std::string &typed = member->second;
                for(const auto &entry : _declarations.membersOf(type)) {
                    const Json item = DeclarationIndex::completionItem(*entry.second, typed);
                    if(!item.isNull())
                        items.push(item);
                    seen.insert(entry.first);
                }
            }
            // 声明索引未命中时回退工作区 class 成员（已打开文档）
            if(type.empty() || items.asArray().empty()) {
                for(const auto &fallback : workspaceClassMembers(type)) {
                    if(!member->second.empty() && !fallback.name.starts_with(member->second))
                        continue;
                    if(seen.contains(fallback.name))
                        continue;
                    seen.insert(fallback.name);
                    Json item = Json::object();
                    item.set("label", fallback.name);
                    item.set("kind",
                             fallback.keyword == "class"          ? 7
                                 : fallback.keyword == "function" ? 2
                                 : fallback.keyword == "property" ? 10
                                                                  : 5);
                    items.push(item);
                }
            }
            return items; // 未知类型宁可空，不猜测成员
        }
        const size_t cursor = lines.offsetAt(at);
        std::string prefix;
        if(cursor != std::string::npos) {
            size_t begin = cursor;
            while(begin > 0 && isIdentifierByte(static_cast<unsigned char>(doc.text[begin - 1])))
                --begin;
            prefix = doc.text.substr(begin, cursor - begin);
        }
        for(const auto &[keyword, docText] : keywordDocs()) {
            if(!prefix.empty() && !keyword.starts_with(prefix))
                continue;
            if(seen.contains(keyword))
                continue;
            seen.insert(keyword);
            Json item = Json::object();
            item.set("label", keyword);
            item.set("kind", 14); // Keyword
            item.set("detail", docText);
            items.push(item);
        }
        for(const auto &declaration : documentDeclarations(lines)) {
            if(!prefix.empty() && !declaration.name.starts_with(prefix))
                continue;
            if(seen.contains(declaration.name))
                continue;
            seen.insert(declaration.name);
            Json item = Json::object();
            item.set("label", declaration.name);
            item.set("kind",
                     declaration.keyword == "class"          ? 7
                         : declaration.keyword == "function" ? 2
                         : declaration.keyword == "property" ? 10
                                                             : 5);
            items.push(item);
        }
        for(const auto &[qualified, entries] : _declarations.entries()) {
            if(entries.size() != 1 || !entries.front().owner.empty())
                continue; // 顶层唯一 API 名才参与；多义保守放弃
            const auto &entry = entries.front();
            if(!prefix.empty() && !entry.symbol.name.starts_with(prefix))
                continue;
            if(seen.contains(entry.symbol.name))
                continue;
            seen.insert(entry.symbol.name);
            const Json item = DeclarationIndex::completionItem(entry, prefix);
            if(!item.isNull())
                items.push(item);
        }
        return items;
    }

    Json Server::onHover(const Json &params) {
        const Json *textDocument = params.find("textDocument");
        const Json *position = params.find("position");
        if(textDocument == nullptr || position == nullptr)
            return Json{};
        const std::string uri = textDocument->stringAt("uri");
        const Document doc = document(uri);
        if(doc.text.empty())
            return Json{};
        refreshDeclarations();
        const Position at{ position->intAt("line"), position->intAt("character") };
        LineIndex lines{ doc.text };
        auto index = documentIndex(doc.text);
        const std::string word = wordAt(doc.text, at);
        if(word.empty())
            return Json{};
        // 成员位置：`receiver.word` 走声明推导
        if(const auto member = index->memberContext(at); member.has_value()) {
            const std::string type = index->inferReceiverType(member->first);
            const auto members = _declarations.membersOf(type);
            const auto entry = members.find(word);
            if(entry != members.end())
                return DeclarationIndex::hover(*entry->second);
            // 工作区 class 成员回退（悬停只展示类别与名字）
            for(const auto &fallback : workspaceClassMembers(type))
                if(fallback.name == word) {
                    Json contents = Json::object();
                    contents.set("kind", "plaintext");
                    contents.set("value", std::string{ kindWord(fallback.keyword) } + " " + fallback.name);
                    Json result = Json::object();
                    result.set("contents", contents);
                    return result;
                }
            return Json{};
        }
        if(const auto keyword = keywordDocs().find(word); keyword != keywordDocs().end()) {
            Json contents = Json::object();
            contents.set("kind", "plaintext");
            contents.set("value", keyword->second);
            Json result = Json::object();
            result.set("contents", contents);
            return result;
        }
        const auto resolved = index->resolve(at);
        if(resolved.binding != nullptr) {
            const auto &binding = *resolved.binding;
            std::string value = std::string{ bindingKindWord(binding) } + " " + word;
            if(!binding.annotation.empty() && binding.annotation != "void")
                value += ": " + binding.annotation;
            Json contents = Json::object();
            contents.set("kind", "plaintext");
            contents.set("value", value);
            Json result = Json::object();
            result.set("contents", contents);
            return result;
        }
        if(!resolved.freeName.empty()) {
            if(const auto *entry = _declarations.unique(resolved.freeName); entry != nullptr)
                return DeclarationIndex::hover(*entry);
            for(const auto &source : workspaceSources())
                if(source.name == word) {
                    std::string value = std::string{ kindWord(source.keyword) } + " " + source.name;
                    Json contents = Json::object();
                    contents.set("kind", "plaintext");
                    contents.set("value", value);
                    Json result = Json::object();
                    result.set("contents", contents);
                    return result;
                }
        }
        return Json{};
    }

    Json Server::onDefinition(const Json &params) {
        const Json *textDocument = params.find("textDocument");
        const Json *position = params.find("position");
        if(textDocument == nullptr || position == nullptr)
            return Json{};
        const std::string uri = textDocument->stringAt("uri");
        const Document doc = document(uri);
        const Position at{ position->intAt("line"), position->intAt("character") };
        if(isDeclarationUri(uri)) {
            // 声明文件内：跳转到该文件中同名符号的名称位置
            std::string word = wordAt(doc.text, at);
            if(word.empty())
                return Json{};
            Json array = Json::array();
            for(const auto &[qualified, entries] : _declarations.entries())
                for(const auto &entry : entries)
                    if(entry.uri == uri && entry.symbol.name == word)
                        array.push(DeclarationIndex::location(entry));
            return array.asArray().size() == 1 ? array.asArray().front() : array;
        }
        if(doc.text.empty())
            return Json{};
        refreshDeclarations();
        auto index = documentIndex(doc.text);
        // 成员位置：`receiver.member` 走声明推导定位（与悬停一致）
        if(const auto member = index->memberContext(at); member.has_value()) {
            const std::string type = index->inferReceiverType(member->first);
            std::string typed = member->second;
            if(typed.empty())
                typed = wordAt(doc.text, at);
            const auto members = _declarations.membersOf(type);
            const auto entry = members.find(typed);
            if(entry != members.end())
                return DeclarationIndex::location(*entry->second);
            for(const auto &fallback : workspaceClassMembers(type))
                if(fallback.name == typed) {
                    Json startJson = Json::object();
                    startJson.set("line", fallback.range.start.line);
                    startJson.set("character", fallback.range.start.character);
                    Json endJson = Json::object();
                    endJson.set("line", fallback.range.end.line);
                    endJson.set("character", fallback.range.end.character);
                    Json rangeJson = Json::object();
                    rangeJson.set("start", startJson);
                    rangeJson.set("end", endJson);
                    Json location = Json::object();
                    location.set("uri", fallback.uri);
                    location.set("range", rangeJson);
                    return location;
                }
            return Json{};
        }
        const auto resolved = index->resolve(at);
        Json array = Json::array();
        if(resolved.binding != nullptr) {
            const auto [start, stop] = index->tokenSpan(resolved.binding->token);
            const Range range = LineIndex{ doc.text }.tokenRange(start, stop);
            Json location = Json::object();
            location.set("uri", uri);
            Json startJson = Json::object();
            startJson.set("line", range.start.line);
            startJson.set("character", range.start.character);
            Json endJson = Json::object();
            endJson.set("line", range.end.line);
            endJson.set("character", range.end.character);
            Json rangeJson = Json::object();
            rangeJson.set("start", startJson);
            rangeJson.set("end", endJson);
            location.set("range", rangeJson);
            return location; // 本文件绑定优先于 API 声明
        }
        if(!resolved.freeName.empty()) {
            for(const auto &[qualified, entries] : _declarations.entries())
                if(qualified == resolved.freeName)
                    for(const auto &entry : entries)
                        array.push(DeclarationIndex::location(entry));
            // 工作区顶层声明（同文件优先，URI 与位置排序），全部返回
            std::vector<WorkspaceSymbol> matches;
            for(const auto &source : workspaceSources())
                if(source.name == resolved.freeName)
                    matches.push_back(source);
            std::ranges::sort(matches, [&](const WorkspaceSymbol &a, const WorkspaceSymbol &b) {
                if((a.uri == uri) != (b.uri == uri))
                    return a.uri == uri;
                if(a.uri != b.uri)
                    return a.uri < b.uri;
                if(a.range.start.line != b.range.start.line)
                    return a.range.start.line < b.range.start.line;
                return a.range.start.character < b.range.start.character;
            });
            for(const auto &match : matches) {
                Json startJson = Json::object();
                startJson.set("line", match.range.start.line);
                startJson.set("character", match.range.start.character);
                Json endJson = Json::object();
                endJson.set("line", match.range.end.line);
                endJson.set("character", match.range.end.character);
                Json rangeJson = Json::object();
                rangeJson.set("start", startJson);
                rangeJson.set("end", endJson);
                Json location = Json::object();
                location.set("uri", match.uri);
                location.set("range", rangeJson);
                array.push(location);
            }
        }
        if(array.asArray().empty())
            return Json{};
        return array.asArray().size() == 1 ? array.asArray().front() : array;
    }

    Json Server::onDocumentSymbol(const Json &params) {
        const Json *textDocument = params.find("textDocument");
        if(textDocument == nullptr)
            return Json::array();
        const Document doc = document(textDocument->stringAt("uri"));
        LineIndex lines{ doc.text };
        Json array = Json::array();
        for(const auto &declaration : documentDeclarations(lines)) {
            Json symbol = Json::object();
            symbol.set("name", declaration.name);
            symbol.set("kind",
                       declaration.keyword == "class"           ? 5
                           : declaration.keyword == "function"  ? 12
                           : declaration.keyword == "property"  ? 7
                           : declaration.keyword == "namespace" ? 3
                           : declaration.keyword == "const"     ? 14
                                                                : 13);
            Json start = Json::object();
            start.set("line", declaration.range.start.line);
            start.set("character", declaration.range.start.character);
            Json end = Json::object();
            end.set("line", declaration.range.end.line);
            end.set("character", declaration.range.end.character);
            Json range = Json::object();
            range.set("start", start);
            range.set("end", end);
            symbol.set("range", range);
            symbol.set("selectionRange", range);
            array.push(symbol);
        }
        return array;
    }

    std::vector<Server::WorkspaceSymbol> Server::workspaceSources() const {
        std::vector<WorkspaceSymbol> results;
        std::vector<std::pair<std::string, std::string>> buffers; // (uri, text)
        {
            std::lock_guard lock{ _mutex };
            for(const auto &[uri, doc] : _documents) {
                const auto path = uriToPath(uri);
                if(!path.empty() && path.extension() == ".tjs")
                    buffers.emplace_back(uri, doc.text);
            }
        }
        std::set<std::filesystem::path> overlayPaths;
        for(const auto &[uri, text] : buffers) {
            if(text.size() > MAX_DOCUMENT_BYTES)
                continue;
            const auto path = uriToPath(uri);
            if(!path.empty())
                overlayPaths.insert(path);
            LineIndex lines{ text };
            for(const auto &declaration : documentDeclarations(lines))
                results.push_back({ declaration.name, declaration.keyword, uri, declaration.range });
        }
        size_t totalBytes = 0;
        size_t fileCount = 0;
        size_t directoryCount = 0;
        for(const auto &root : _roots) {
            if(fileCount >= MAX_WORKSPACE_FILES || directoryCount >= MAX_WORKSPACE_DIRECTORIES)
                break;
            std::error_code ec;
            for(std::filesystem::recursive_directory_iterator
                    it{ root,
                        std::filesystem::directory_options::skip_permission_denied |
                            std::filesystem::directory_options::follow_directory_symlink,
                        ec },
                end;
                !ec && it != end && fileCount < MAX_WORKSPACE_FILES; it.increment(ec)) {
                if(it->is_directory(ec)) {
                    ++directoryCount;
                    if(directoryCount > MAX_WORKSPACE_DIRECTORIES)
                        break;
                    if(excludedDirectories().contains(utf8Path(it->path().filename())))
                        it.disable_recursion_pending();
                    continue;
                }
                if(it->path().extension() != ".tjs" || overlayPaths.contains(it->path()))
                    continue;
                std::error_code sizeError;
                const auto size = it->file_size(sizeError);
                if(sizeError || size > MAX_DOCUMENT_BYTES || totalBytes + size > MAX_WORKSPACE_BYTES)
                    continue;
                std::ifstream input{ it->path(), std::ios::binary };
                if(!input)
                    continue;
                std::string text{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
                if(text.starts_with("\xEF\xBB\xBF"))
                    text.erase(0, 3);
                ++fileCount;
                totalBytes += text.size();
                LineIndex lines{ text };
                for(const auto &declaration : documentDeclarations(lines))
                    results.push_back(
                        { declaration.name, declaration.keyword, pathToUri(it->path()), declaration.range });
            }
            if(ec)
                break;
        }
        std::ranges::sort(results, [](const WorkspaceSymbol &a, const WorkspaceSymbol &b) {
            if(a.name != b.name)
                return a.name < b.name;
            if(a.uri != b.uri)
                return a.uri < b.uri;
            if(a.range.start.line != b.range.start.line)
                return a.range.start.line < b.range.start.line;
            return a.range.start.character < b.range.start.character;
        });
        if(results.size() > MAX_WORKSPACE_SYMBOLS)
            results.resize(MAX_WORKSPACE_SYMBOLS);
        return results;
    }

    Json Server::onWorkspaceSymbol(const Json &params) {
        const std::string query = params.stringAt("query");
        std::string lowerQuery = query;
        std::ranges::transform(lowerQuery, lowerQuery.begin(),
                               [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        Json array = Json::array();
        for(const auto &symbol : workspaceSources()) {
            std::string lowerName = symbol.name;
            std::ranges::transform(lowerName, lowerName.begin(),
                                   [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if(lowerName.find(lowerQuery) == std::string::npos)
                continue;
            Json item = Json::object();
            item.set("name", symbol.name);
            item.set("kind",
                     symbol.keyword == "class"           ? 5
                         : symbol.keyword == "function"  ? 12
                         : symbol.keyword == "property"  ? 7
                         : symbol.keyword == "namespace" ? 3
                         : symbol.keyword == "const"     ? 14
                                                         : 13);
            Json start = Json::object();
            start.set("line", symbol.range.start.line);
            start.set("character", symbol.range.start.character);
            Json end = Json::object();
            end.set("line", symbol.range.end.line);
            end.set("character", symbol.range.end.character);
            Json range = Json::object();
            range.set("start", start);
            range.set("end", end);
            Json location = Json::object();
            location.set("uri", symbol.uri);
            location.set("range", range);
            item.set("location", location);
            array.push(item);
        }
        return array;
    }
} // namespace cial::lsp
