#!/usr/bin/env python3
"""Dependency-free stdio LSP server backed by Ciallang's compile-only checker."""
import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
from urllib.request import url2pathname

KEYWORDS = {
    "function": "定义函数：function name(parameters) { ... }",
    "var": "声明变量：var name = value;",
    "const": "声明带初值的变量；当前 TJS2 兼容语义仍允许后续赋值。",
    "class": "定义类：class Name extends Base { ... }",
    "property": "定义具有 getter / setter 的属性。",
    "try": "try { ... } catch(e) { ... }",
    "with": "with(object) { .member; }",
    "return": "返回函数结果。",
    "if": "if(condition) statement; else statement;",
    "while": "while(condition) statement;",
    "for": "for(init; condition; step) statement;",
    "switch": "switch(value) { case value: ...; break; default: ...; }",
    "new": "创建类或内置容器实例。",
    "throw": "抛出可由 catch 捕获的脚本异常。",
    "this": "当前调用上下文。",
    "super": "父类上下文。",
    "global": "全局对象。",
    "void": "空值。",
    "true": "逻辑真（整数 1）。",
    "false": "逻辑假（整数 0）。",
    "Array": "数组；支持字面量 []、下标、push、sort 等。",
    "Dictionary": "字典；支持字面量 %[]。",
    "String": "字符串与字符串原生方法。",
    "Math": "数学方法、常量与随机数。",
    "Date": "日期构造、解析及分量方法。",
    "RegExp": "正则表达式，支持 /pattern/flags 字面量。",
    "Exception": "异常对象，包含 message 成员。",
}
DECLARATION = re.compile(r"\b(function|var|const|class|property)\s+([\w$]+)")
WORD = re.compile(r"[\w$]+")
SYMBOL_KINDS = {"function": 12, "class": 5, "property": 7}
MAX_DOCUMENT_BYTES = 2_000_000
MAX_WORKSPACE_BYTES = 20_000_000
MAX_WORKSPACE_FILES = 2000
MAX_WORKSPACE_DIRECTORIES = 10000
MAX_WORKSPACE_SYMBOLS = 200
EXCLUDED_DIRECTORIES = {".git", ".idea", ".vscode", ".venv", ".cache", "build", "bin", "ext", "node_modules", "__pycache__"}


def file_path(uri):
    parsed = urlsplit(uri)
    if parsed.scheme != "file" or parsed.query or parsed.fragment:
        return None
    value = parsed.path
    if parsed.netloc and parsed.netloc != "localhost":
        value = "//" + parsed.netloc + value
    path = pathlib.Path(url2pathname(value))
    if not path.is_absolute():
        return None
    try:
        return path.resolve()
    except (OSError, RuntimeError):
        return None


def declarations(text):
    """Lexical declarations, with literal/comment bodies hidden but offsets preserved."""
    masked = list(text)
    index, expression = 0, True
    while index < len(text):
        start = index
        character = text[index]
        if character.isspace():
            index += 1
            continue
        if text.startswith("//", index):
            index = text.find("\n", index)
            if index < 0:
                index = len(text)
        elif text.startswith("/*", index):
            end = text.find("*/", index + 2)
            index = len(text) if end < 0 else end + 2
        elif character in "\"'" or (character == "/" and expression):
            quote, in_class = character, False
            index += 1
            while index < len(text):
                current = text[index]
                index += 1
                if current == "\\":
                    index = min(index + 1, len(text))
                elif quote == "/" and current == "[":
                    in_class = True
                elif quote == "/" and current == "]":
                    in_class = False
                elif current == quote and not in_class:
                    break
            expression = False
        else:
            word = WORD.match(text, index)
            if word:
                index = word.end()
                expression = word[0] in {"return", "throw", "case", "delete", "typeof", "in"}
            else:
                expression = character not in ")]}."
                index += 1
            continue
        for offset in range(start, index):
            if masked[offset] != "\n":
                masked[offset] = " "
    source = "".join(masked)
    symbols = []
    offset, row, column = 0, 0, 0
    for match in DECLARATION.finditer(source):
        before = text[offset:match.start(2)]
        if "\n" in before:
            row += before.count("\n")
            column = utf16(before.rsplit("\n", 1)[-1])
        else:
            column += utf16(before)
        offset = match.start(2)
        span = {"start": {"line": row, "character": column},
                "end": {"line": row, "character": column + utf16(match[2])}}
        symbols.append({"name": match[2], "kind": SYMBOL_KINDS.get(match[1], 13),
                        "range": span, "selectionRange": span})
    return symbols


def utf16(text):
    return len(text.encode("utf-16-le")) // 2


def source_position(text, line, byte_column):
    """Native diagnostics use UTF-8 byte columns; LSP positions use UTF-16."""
    lines = text.split("\n")
    line = min(max(0, line), len(lines) - 1)
    prefix = lines[line].encode("utf-8")[:max(0, byte_column)].decode("utf-8", errors="ignore")
    return {"line": line, "character": utf16(prefix)}


def line_prefix(text, position):
    lines = text.split("\n")
    line = lines[min(max(position.get("line", 0), 0), len(lines) - 1)]
    units = max(0, position.get("character", 0))
    prefix = ""
    for character in line:
        if utf16(prefix + character) > units:
            break
        prefix += character
    return prefix


class Server:
    def __init__(self, cial, source=None, sink=None):
        self.cial = shutil.which(cial) or str(pathlib.Path(cial).resolve())
        self.source = source or sys.stdin.buffer
        self.sink = sink or sys.stdout.buffer
        self.documents = {}
        self.workspace_roots = set()
        self.symbol_cache = {}
        self.initialized = False
        self.shutdown = False

    def send(self, payload):
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.sink.write(b"Content-Length: " + str(len(body)).encode("ascii") + b"\r\n\r\n" + body)
        self.sink.flush()

    def result(self, identifier, value):
        self.send({"jsonrpc": "2.0", "id": identifier, "result": value})

    def error(self, identifier, code, message):
        self.send({"jsonrpc": "2.0", "id": identifier, "error": {"code": code, "message": message}})

    def update_workspace_folders(self, added, removed=()):
        for folders, operation in ((removed, self.workspace_roots.discard), (added, self.workspace_roots.add)):
            for folder in folders:
                path = file_path(folder["uri"])
                if path is not None:
                    operation(path)
        self.symbol_cache.clear()

    def workspace_symbols(self, query):
        def in_workspace(path):
            return not self.workspace_roots or any(path == root or root in path.parents for root in self.workspace_roots)

        # Canonical paths make an unsaved buffer override its on-disk spelling/URI.
        overlays = {}
        for uri, document in self.documents.items():
            path = file_path(uri)
            if path is not None and path.suffix.lower() == ".tjs" and in_workspace(path):
                overlays[path] = document["text"]
        sources, visited, total_bytes, file_count, directory_count = [], {}, 0, 0, 0
        # Reserve the budget for current buffers before looking at disk files.
        for path, text in sorted(overlays.items()):
            size = len(text.encode("utf-8"))
            if size <= MAX_DOCUMENT_BYTES and total_bytes + size <= MAX_WORKSPACE_BYTES:
                total_bytes += size
                sources.append((path.as_uri(), declarations(text)))
        for root in sorted(self.workspace_roots):
            if file_count >= MAX_WORKSPACE_FILES or directory_count >= MAX_WORKSPACE_DIRECTORIES:
                break
            for directory, folders, names in os.walk(root, followlinks=False):
                directory_count += 1
                folders[:] = sorted(name for name in folders if name not in EXCLUDED_DIRECTORIES)
                for name in sorted(names):
                    if file_count >= MAX_WORKSPACE_FILES:
                        break
                    if not name.lower().endswith(".tjs"):
                        continue
                    file_count += 1
                    try:
                        path = (pathlib.Path(directory) / name).resolve()
                        if not in_workspace(path) or path in visited or path in overlays:
                            continue
                        stat = path.stat()
                        if stat.st_size > MAX_DOCUMENT_BYTES or total_bytes + stat.st_size > MAX_WORKSPACE_BYTES:
                            continue
                        signature = (stat.st_mtime_ns, stat.st_size)
                        size = stat.st_size
                        cached = self.symbol_cache.get(path)
                        if cached and cached[0] == signature:
                            symbols = cached[1]
                        else:
                            with path.open("rb") as stream:
                                data = stream.read(MAX_DOCUMENT_BYTES + 1)
                            if len(data) > MAX_DOCUMENT_BYTES:
                                continue
                            size = len(data)
                            symbols = declarations(data.decode("utf-8-sig"))
                    except (OSError, UnicodeError, RuntimeError):
                        continue
                    if total_bytes + size > MAX_WORKSPACE_BYTES:
                        continue
                    total_bytes += size
                    visited[path] = (signature, symbols)
                    sources.append((path.as_uri(), symbols))
                if file_count >= MAX_WORKSPACE_FILES or directory_count >= MAX_WORKSPACE_DIRECTORIES:
                    break
        self.symbol_cache = visited
        query = query.casefold()
        results = [{"name": symbol["name"], "kind": symbol["kind"],
                    "location": {"uri": uri, "range": symbol["selectionRange"]}}
                   for uri, symbols in sources for symbol in symbols if query in symbol["name"].casefold()]
        results.sort(key=lambda symbol: (symbol["name"].casefold(), symbol["location"]["uri"],
                                         symbol["location"]["range"]["start"]["line"],
                                         symbol["location"]["range"]["start"]["character"]))
        return results[:MAX_WORKSPACE_SYMBOLS]

    def check(self, text):
        with tempfile.TemporaryDirectory(prefix="cial-lsp-") as directory:
            path = pathlib.Path(directory) / "document.tjs"
            path.write_text(text, encoding="utf-8")
            try:
                run = subprocess.run([self.cial, "--check-json", str(path)], capture_output=True, timeout=5)
                report = json.loads(run.stdout.decode("utf-8"))
                if run.returncode not in (0, 1):
                    raise ValueError("checker exited with status " + str(run.returncode))
                diagnostics = []
                for diagnostic in report["diagnostics"]:
                    start = source_position(text, diagnostic["line"], diagnostic["column"])
                    end = source_position(text, diagnostic["endLine"], diagnostic["endColumn"] + 1)
                    if (end["line"], end["character"]) < (start["line"], start["character"]):
                        end = start.copy()
                    diagnostics.append({"range": {"start": start, "end": end},
                                        "severity": diagnostic["severity"], "source": "ciallang",
                                        "message": re.sub(r"^\([^@]*@\d+:\d+\)\s*", "", diagnostic["message"])})
                return diagnostics
            except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
                return [{"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 0}},
                         "severity": 1, "source": "ciallang", "message": "Syntax checker unavailable: " + str(error)}]

    def publish(self, uri, text, version):
        diagnostics = self.check(text) if len(text.encode("utf-8")) <= 2_000_000 else [
            {"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 0}},
             "severity": 2, "source": "ciallang", "message": "Document exceeds the 2 MB diagnostics limit."}]
        self.send({"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics",
                   "params": {"uri": uri, "version": version, "diagnostics": diagnostics}})

    def handle(self, request):
        if request.get("jsonrpc") != "2.0":
            self.error(request.get("id"), -32600, "Expected JSON-RPC 2.0")
            return None
        method = request.get("method")
        params = request.get("params", {})
        identifier = request.get("id")
        if method == "exit":
            return 0 if self.shutdown else 1
        if method == "initialize":
            if self.initialized or self.shutdown:
                self.error(identifier, -32600, "Server was already initialized")
                return None
            self.initialized = True
            folders = params.get("workspaceFolders")
            if folders is None:
                root_uri = params.get("rootUri")
                if root_uri:
                    folders = [{"uri": root_uri}]
                elif params.get("rootPath"):
                    folders = [{"uri": pathlib.Path(params["rootPath"]).resolve().as_uri()}]
            self.update_workspace_folders(folders or [])
            self.result(identifier, {"capabilities": {"positionEncoding": "utf-16", "textDocumentSync": {"openClose": True, "change": 1},
                                                     "completionProvider": {}, "hoverProvider": True,
                                                     "documentSymbolProvider": True, "workspaceSymbolProvider": True,
                                                     "workspace": {"workspaceFolders": {"supported": True,
                                                                                         "changeNotifications": True}}},
                                     "serverInfo": {"name": "Ciallang LSP", "version": "0.2.0"}})
        elif not self.initialized or self.shutdown:
            if "id" in request:
                self.error(identifier, -32002 if not self.initialized else -32600, "Server is not active")
        elif method == "shutdown":
            self.shutdown = True
            self.result(identifier, None)
        elif method == "workspace/didChangeWorkspaceFolders":
            event = params["event"]
            self.update_workspace_folders(event.get("added", []), event.get("removed", []))
        elif method == "workspace/symbol":
            query = params.get("query", "")
            if not isinstance(query, str):
                raise ValueError("Workspace symbol query must be a string")
            self.result(identifier, self.workspace_symbols(query))
        elif method in ("initialized", "$/cancelRequest", "$/setTrace"):
            pass
        elif method in ("textDocument/didOpen", "textDocument/didChange"):
            document = params["textDocument"]
            uri, version = document["uri"], document["version"]
            old = self.documents.get(uri)
            if old and version <= old["version"]:
                return None
            if method.endswith("didOpen"):
                text = document["text"]
            else:
                changes = params["contentChanges"]
                if not changes or any("range" in change for change in changes):
                    return None  # 仅声明完整文本同步，不接受错误的增量更新。
                text = changes[-1]["text"]
            self.documents[uri] = {"text": text, "version": version}
            self.publish(uri, text, version)
        elif method == "textDocument/didClose":
            uri = params["textDocument"]["uri"]
            self.documents.pop(uri, None)
            self.send({"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics",
                       "params": {"uri": uri, "diagnostics": []}})
        elif method in ("textDocument/completion", "textDocument/hover", "textDocument/documentSymbol"):
            document = self.documents.get(params["textDocument"]["uri"], {"text": ""})
            text = document["text"]
            if method.endswith("completion"):
                names = set(KEYWORDS) | {symbol["name"] for symbol in declarations(text)}
                prefix = line_prefix(text, params["position"])
                match = re.search(r"[\w$]+$", prefix)
                prefix = match[0] if match else ""
                self.result(identifier, [{"label": name, "kind": 14 if name in KEYWORDS else 6,
                                          "detail": KEYWORDS.get(name, "document declaration")}
                                         for name in sorted(names) if name.startswith(prefix)])
            elif method.endswith("hover"):
                prefix = line_prefix(text, params["position"])
                line = text.split("\n")[params["position"]["line"]]
                column = len(prefix)
                match = next((match for match in WORD.finditer(line) if match.start() <= column < match.end()), None)
                help_text = KEYWORDS.get(match[0]) if match else None
                self.result(identifier, {"contents": {"kind": "plaintext", "value": help_text}} if help_text else None)
            else:
                self.result(identifier, declarations(text))
        elif "id" in request:
            self.error(identifier, -32601, "Method not found: " + str(method))
        return None

    def run(self):
        while True:
            headers = {}
            while True:
                line = self.source.readline()
                if not line:
                    return 0
                if line in (b"\r\n", b"\n"):
                    break
                key, value = line.decode("ascii").split(":", 1)
                headers[key.lower().strip()] = value.strip()
            length = int(headers.get("content-length", "-1"))
            if not 0 <= length <= 8_000_000:
                raise ValueError("Invalid Content-Length")
            body = self.source.read(length)
            if len(body) != length:
                return 1
            request = {}
            try:
                request = json.loads(body)
                if not isinstance(request, dict):
                    raise ValueError("Expected a request object")
                result = self.handle(request)
                if result is not None:
                    return result
            except json.JSONDecodeError as error:
                self.error(None, -32700, str(error))
            except (ValueError, KeyError, IndexError, TypeError) as error:
                self.error(request.get("id") if isinstance(request, dict) else None, -32602, str(error))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cial", required=True, help="Path to the built cial executable")
    args = parser.parse_args()
    return Server(args.cial).run()


if __name__ == "__main__":
    raise SystemExit(main())
