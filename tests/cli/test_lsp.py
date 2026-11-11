"""cial-lsp 进程内语言服务器协议回归（doc/lsp.md）。

运行：python tests/cli/test_lsp.py --lsp <cial-lsp 路径> [--cial <cial 路径>]
通过真实 stdio 子进程覆盖：握手与退出、进程内编译诊断（含中文行 UTF-16 换算、
修复后清空）、关键字与文档声明补全、声明成员补全（签名 + /** */ 文档）、工作区
class 成员回退、悬停、跳转定义（本文件绑定 / 声明文件 / 跨文件唯一与多义）、
文档符号与工作区符号。系统声明（构建期生成在 cial-lsp 可执行文件旁的 declarations/
或安装 share 布局）是成员补全与跳转的数据源，本测试同时验证自动发现。
"""
import argparse
import json
import pathlib
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.parse

parser = argparse.ArgumentParser()
parser.add_argument("--lsp", required=True)
parser.add_argument("--cial", default="")
args, rest = parser.parse_known_args()

EXCLUDED_TIMEOUT = 15


def uri_of(path):
    return pathlib.Path(path).as_uri()


class LspClient:
    def __init__(self, executable):
        self.process = subprocess.Popen([executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)

    def send(self, payload):
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.process.stdin.write(b"Content-Length: " + str(len(data)).encode("ascii") + b"\r\n\r\n" + data)
        self.process.stdin.flush()

    def receive(self):
        headers = {}
        while True:
            line = self.process.stdout.readline()
            if not line:
                raise AssertionError("server exited before response")
            if line in (b"\r\n", b"\n"):
                break
            key, value = line.decode("ascii").split(":", 1)
            headers[key.lower()] = value.strip()
        return json.loads(self.process.stdout.read(int(headers["content-length"])))

    def request(self, identifier, method, params=None):
        self.send({"jsonrpc": "2.0", "id": identifier, "method": method, "params": params or {}})
        deadline = time.monotonic() + EXCLUDED_TIMEOUT
        while time.monotonic() < deadline:
            message = self.receive()
            if message.get("id") == identifier:
                if "error" in message:
                    raise AssertionError(f"{method} failed: {message['error']}")
                return message["result"]
        raise TimeoutError(method)

    def open(self, uri, text, version=1):
        self.send({"jsonrpc": "2.0", "method": "textDocument/didOpen",
                   "params": {"textDocument": {"uri": uri, "version": version, "languageId": "ciallang",
                                               "text": text}}})

    def change(self, uri, text, version):
        self.send({"jsonrpc": "2.0", "method": "textDocument/didChange",
                   "params": {"textDocument": {"uri": uri, "version": version},
                              "contentChanges": [{"text": text}]}})

    def diagnostics(self, timeout=10.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            message = self.receive()
            if message.get("method") == "textDocument/publishDiagnostics":
                return message["params"]["diagnostics"]
        raise TimeoutError("diagnostics")

    def close(self):
        self.request(900, "shutdown")
        self.send({"jsonrpc": "2.0", "method": "exit"})
        return self.process.wait(timeout=5)


class LspProtocolTest(unittest.TestCase):
    def setUp(self):
        self.client = LspClient(args.lsp)
        self.directory = tempfile.TemporaryDirectory(prefix="cial lsp %20 中文 ")
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name).resolve()
        self.addCleanup(self.client.process.kill)
        result = self.client.request(1, "initialize", {
            "capabilities": {},
            "workspaceFolders": [{"uri": uri_of(self.root), "name": "workspace"}],
            "initializationOptions": {}})
        capabilities = result["capabilities"]
        self.assertEqual(capabilities["positionEncoding"], "utf-16")
        self.assertTrue(capabilities["definitionProvider"])
        self.client.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

    def tearDown(self):
        pass

    def open_document(self, name, text, wait_diagnostics=True):
        path = self.root / name
        path.write_text(text, encoding="utf-8", newline="\n")
        uri = uri_of(path)
        self.client.open(uri, text)
        if wait_diagnostics:
            self.client.diagnostics()
        return uri

    def test_diagnostics_report_and_clear(self):
        uri = uri_of(self.root / "diagnostics.tjs")
        self.client.open(uri, "var value = ;")
        published = self.client.diagnostics()
        self.assertTrue(published)
        self.assertEqual(published[0]["severity"], 1)
        self.assertTrue(published[0]["message"])
        # 中文行的字节列 → UTF-16 半开范围
        text = 'var 文本 = "值";\nvar broken = ;'
        self.client.change(uri, text, version=2)
        published = self.client.diagnostics()
        self.assertTrue(published)
        self.assertEqual(published[0]["range"]["start"]["line"], 1)
        # 修复后发布空诊断
        self.client.change(uri, "var ok = 1;", version=3)
        published = self.client.diagnostics()
        self.assertEqual(published, [])

    def test_keyword_and_document_completion(self):
        uri = self.open_document("completion.tjs", "function foobar() {}\nvar value = 1;\n")
        items = self.client.request(2, "textDocument/completion",
                                    {"textDocument": {"uri": uri}, "position": {"line": 2, "character": 0}})
        labels = {item["label"]: item for item in items}
        self.assertIn("foobar", labels)
        self.assertIn("function", labels)  # 纯语言关键字（无 API 事实）
        # Array 来自声明索引顶层补全（kind 7 Class + 类级文档），不在关键字表里
        self.assertEqual(labels["Array"]["kind"], 7)
        self.assertIn("动态数组", labels["Array"]["documentation"]["value"])
        # 前缀过滤
        self.client.change(uri, "var x = Ar", version=2)
        self.client.diagnostics()
        items = self.client.request(3, "textDocument/completion",
                                    {"textDocument": {"uri": uri}, "position": {"line": 0, "character": 10}})
        self.assertIn("Array", {item["label"] for item in items})
        self.assertNotIn("String", {item["label"] for item in items})

    def test_member_completion_from_system_declarations_with_docs(self):
        uri = self.open_document("members.tjs", "var values = [];\nvalues.push;")
        position = {"line": 1, "character": 7}
        items = self.client.request(4, "textDocument/completion",
                                    {"textDocument": {"uri": uri}, "position": position})
        by_label = {item["label"]: item for item in items}
        self.assertIn("push", by_label)
        self.assertEqual(by_label["push"]["detail"], "push(values: any*): Integer")
        self.assertIn("末尾追加", by_label["push"]["documentation"]["value"])
        # 未知接收者不猜成员
        self.open_document("unknown.tjs", "var mystery = foo();\nmystery.bar;")
        items = self.client.request(5, "textDocument/completion",
                                    {"textDocument": {"uri": uri_of(self.root / "unknown.tjs")},
                                     "position": {"line": 1, "character": 11}})
        self.assertEqual(items, [])

    def test_hover_shows_declaration_signature_and_doc(self):
        uri = self.open_document("hover.tjs", "var values = [];\nvalues.pop;")
        hover = self.client.request(6, "textDocument/hover",
                                    {"textDocument": {"uri": uri}, "position": {"line": 1, "character": 7}})
        value = hover["contents"]["value"]
        self.assertIn("pop", value)
        self.assertIn("移除并返回末元素", value)
        # 关键字悬停
        self.open_document("kw.tjs", "function a() {}")
        hover = self.client.request(7, "textDocument/hover",
                                    {"textDocument": {"uri": uri_of(self.root / "kw.tjs")},
                                     "position": {"line": 0, "character": 3}})
        self.assertIn("定义函数", hover["contents"]["value"])

    def test_definition_jumps_to_local_binding_workspace_and_declarations(self):
        main_uri = self.open_document("main.tjs", "var shared = 1;\nvar alias = shared;\nshared;")
        location = self.client.request(8, "textDocument/definition",
                                       {"textDocument": {"uri": main_uri}, "position": {"line": 1, "character": 12}})
        self.assertEqual(location["uri"], main_uri)
        self.assertEqual(location["range"]["start"], {"line": 0, "character": 4})
        # 跨文件唯一顶层声明
        other = self.root / "other.tjs"
        other.write_text("function uniqueHelper() {}\n", encoding="utf-8", newline="\n")
        self.client.change(main_uri, "uniqueHelper;", version=2)
        self.client.diagnostics()
        location = self.client.request(9, "textDocument/definition",
                                       {"textDocument": {"uri": main_uri}, "position": {"line": 0, "character": 5}})
        self.assertEqual(location["uri"], uri_of(other))
        # 声明文件跳转：Array.push → 构建期生成的 Array.d.tjs 真实文件
        self.client.change(main_uri, "var a = [];\na.push;", version=3)
        self.client.diagnostics()
        location = self.client.request(10, "textDocument/definition",
                                       {"textDocument": {"uri": main_uri}, "position": {"line": 1, "character": 2}})
        self.assertTrue(location["uri"].endswith("/Array.d.tjs"), location["uri"])
        path = pathlib.Path(urllib.parse.unquote(location["uri"].removeprefix("file:///")))
        self.assertTrue(path.is_file())
        line = path.read_text(encoding="utf-8").splitlines()[location["range"]["start"]["line"]]
        self.assertIn("function push(values: any*): Integer;", line)

    def test_workspace_symbols_and_document_symbols(self):
        self.open_document("symbols.tjs", "class Widget { function paint(): void; }\nvar gadget = 1;")
        uri = uri_of(self.root / "symbols.tjs")
        symbols = self.client.request(11, "textDocument/documentSymbol", {"textDocument": {"uri": uri}})
        self.assertEqual({symbol["name"] for symbol in symbols}, {"Widget", "paint", "gadget"})
        matches = self.client.request(12, "workspace/symbol", {"query": "widget"})
        self.assertTrue(any(symbol["name"] == "Widget" and symbol["location"]["uri"] == uri
                            for symbol in matches))

    def test_member_completion_falls_back_to_workspace_class(self):
        self.open_document("widget.tjs",
                           "class Widget { /** 刷新显示 */ function refresh(): void; }\nvar widget = new Widget();\nwidget.refresh;")
        uri = uri_of(self.root / "widget.tjs")
        items = self.client.request(13, "textDocument/completion",
                                    {"textDocument": {"uri": uri}, "position": {"line": 2, "character": 7}})
        self.assertIn("refresh", {item["label"] for item in items})
        location = self.client.request(14, "textDocument/definition",
                                       {"textDocument": {"uri": uri}, "position": {"line": 2, "character": 7}})
        self.assertEqual(location["uri"], uri)
        self.assertEqual(location["range"]["start"], {"line": 0, "character": 36})

    def test_shutdown_and_exit_code(self):
        self.assertEqual(self.client.close(), 0)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *rest])
