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

    def dispose(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.communicate(timeout=5)


class LspProtocolTest(unittest.TestCase):
    def setUp(self):
        self.client = LspClient(args.lsp)
        self.directory = tempfile.TemporaryDirectory(prefix="cial lsp %20 中文 ")
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name).resolve()
        self.addCleanup(self.client.dispose)
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
        self.client.send({'jsonrpc': '2.0', 'method': 'workspace/didChangeWatchedFiles',
                          'params': {'changes': [{'uri': uri_of(other), 'type': 1}]}})
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
        decoded = urllib.parse.unquote(urllib.parse.urlsplit(location["uri"]).path)
        if sys.platform == "win32" and len(decoded) > 2 and decoded[2] == ':':
            decoded = decoded[1:]
        path = pathlib.Path(decoded)
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

    def test_qualified_class_and_namespace_members_jump_to_declarations(self):
        uri = self.open_document("qualified.tjs", "Array.push;\nMath.abs;")
        for identifier, line, character, filename, signature in [
                (40, 0, 7, "Array.d.tjs", "push(values: any*): Integer"),
                (41, 1, 7, "Math.d.tjs", "abs(")]:
            with self.subTest(filename=filename):
                params = {"textDocument": {"uri": uri}, "position": {"line": line, "character": character}}
                location = self.client.request(identifier, "textDocument/definition", params)
                self.assertIsNotNone(location)
                self.assertTrue(location["uri"].endswith('/' + filename), location)
                hover = self.client.request(identifier + 10, "textDocument/hover", params)
                self.assertIn(signature, hover["contents"]["value"])
        # 同名局部变量仍按绑定推导，不能直接套用系统类/命名空间。
        self.client.change(uri, "var Array = unknown();\nArray.push;", version=2)
        self.client.diagnostics()
        location = self.client.request(60, "textDocument/definition", {
            "textDocument": {"uri": uri}, "position": {"line": 1, "character": 7}})
        self.assertIsNone(location)

    def test_generated_sdk_declarations_do_not_shadow_system_or_project_declarations(self):
        for directory in ('build/Debug/declarations', 'build/Release/declarations', 'out/sdk/types',
                          'node_modules/sdk', 'ext/sdk', 'bin/cache', '.git/cache'):
            target = self.root / directory / 'Array.d.tjs'
            target.parent.mkdir(parents=True)
            target.write_text('class Array { function push(value: any): Integer; }', encoding='utf-8')
        uri = self.open_document('arrays.tjs', 'var values = [];\nvalues.push(1);')
        params = {'textDocument': {'uri': uri}, 'position': {'line': 1, 'character': 8}}
        location = self.client.request(70, 'textDocument/definition', params)
        self.assertIsNotNone(location)
        self.assertTrue(location['uri'].endswith('/Array.d.tjs'))
        self.assertNotIn(uri_of(self.root) + '/', location['uri'])
        # 实际项目声明仍可覆盖系统声明，过滤只针对自动扫描的产物目录。
        declaration = self.root / 'types/host.d.tjs'
        declaration.parent.mkdir()
        declaration.write_text('class Array { function push(value: any): Integer; }', encoding='utf-8')
        self.client.send({'jsonrpc': '2.0', 'method': 'workspace/didChangeWatchedFiles',
                          'params': {'changes': [{'uri': uri_of(declaration), 'type': 1}]}})
        location = self.client.request(71, 'textDocument/definition', params)
        self.assertEqual(location['uri'], uri_of(declaration))

    def test_index_reuses_unchanged_files_and_invalidates_one_declaration(self):
        declaration = self.root / 'types/Widget.D.TJS'
        declaration.parent.mkdir()
        declaration.write_text('class Widget { function open(): String; }', encoding='utf-8')
        self.client.send({'jsonrpc': '2.0', 'method': 'workspace/didChangeWatchedFiles',
                          'params': {'changes': [{'uri': uri_of(declaration), 'type': 1}]}})
        uri = self.open_document('indexed.tjs', 'var box = new Widget();\nbox.open;')
        params = {'textDocument': {'uri': uri}, 'position': {'line': 1, 'character': 5}}
        self.assertEqual(self.client.request(80, 'textDocument/definition', params)['uri'], uri_of(declaration))
        before = self.client.request(81, 'ciallang/indexStatus')
        for identifier in range(100, 130):
            self.assertEqual(self.client.request(identifier, 'textDocument/definition', params)['uri'], uri_of(declaration))
        after = self.client.request(82, 'ciallang/indexStatus')
        for key in ('reads', 'lexicalBuilds', 'declarationParses'):
            self.assertEqual(before[key], after[key], key)
        declaration.write_text('class Widget { function open(): Integer; }', encoding='utf-8')
        self.client.send({'jsonrpc': '2.0', 'method': 'workspace/didChangeWatchedFiles',
                          'params': {'changes': [{'uri': uri_of(declaration), 'type': 2}]}})
        hover = self.client.request(83, 'textDocument/hover', params)
        self.assertIn('Integer', hover['contents']['value'])
        changed = self.client.request(84, 'ciallang/indexStatus')
        self.assertEqual(changed['declarationParses'], after['declarationParses'] + 1)
        self.assertEqual(changed['lexicalBuilds'], after['lexicalBuilds'])
        declaration.unlink()
        self.client.send({'jsonrpc': '2.0', 'method': 'workspace/didChangeWatchedFiles',
                          'params': {'changes': [{'uri': uri_of(declaration), 'type': 3}]}})
        self.assertIsNone(self.client.request(85, 'textDocument/definition', params))

    def test_exception_class_reference_has_one_class_target(self):
        self.open_document('exception-implementation.tjs', 'class Exception { function Exception(message) {} }')
        uri = self.open_document('exception-use.tjs', 'var failure = new Exception("failed");\nException("other");')
        for identifier, line, character in [(90, 0, 22), (91, 1, 3)]:
            location = self.client.request(identifier, 'textDocument/definition', {
                'textDocument': {'uri': uri}, 'position': {'line': line, 'character': character}})
            self.assertIsInstance(location, dict)
            self.assertTrue(location['uri'].endswith('/Exception.d.tjs'))
            decoded = urllib.parse.unquote(urllib.parse.urlsplit(location['uri']).path)
            if sys.platform == 'win32' and decoded[2:3] == ':': decoded = decoded[1:]
            target_line = pathlib.Path(decoded).read_text(encoding='utf-8').splitlines()[location['range']['start']['line']]
            self.assertIn('class Exception', target_line)
        symbols = self.client.request(92, 'workspace/symbol', {'query': 'Exception'})
        self.assertTrue(all(item['kind'] == 5 for item in symbols))

    def test_definition_and_symbol_search_reach_names_beyond_display_limit(self):
        definitions = '\n'.join(f'function helper{i:03}() {{}}' for i in range(300))
        target = self.open_document('many-symbols.tjs', definitions)
        uri = self.open_document('lookup-last.tjs', 'helper299;')
        location = self.client.request(93, 'textDocument/definition', {
            'textDocument': {'uri': uri}, 'position': {'line': 0, 'character': 4}})
        self.assertEqual(location['uri'], target)
        self.assertEqual(location['range']['start']['line'], 299)
        symbols = self.client.request(94, 'workspace/symbol', {'query': 'helper299'})
        self.assertEqual(len(symbols), 1)
        self.assertEqual(symbols[0]['name'], 'helper299')

    def test_qualified_type_reference_in_declaration_avoids_global_class_with_same_name(self):
        text = 'namespace Host { class Exception {} }\nfunction create(): Host.Exception;'
        uri = self.open_document('qualified-types.d.tjs', text)
        location = self.client.request(95, 'textDocument/definition', {
            'textDocument': {'uri': uri}, 'position': {'line': 1, 'character': 27}})
        self.assertEqual(location['uri'], uri)
        self.assertEqual(location['range']['start']['line'], 0)

    def test_explicit_declaration_file_inside_excluded_directory_remains_available(self):
        declaration = self.root / 'out/host/Host.d.tjs'
        declaration.parent.mkdir(parents=True)
        declaration.write_text('function explicitOnly(): Integer;', encoding='utf-8')
        client = LspClient(args.lsp)
        self.addCleanup(client.dispose)
        client.request(1, 'initialize', {
            'capabilities': {}, 'workspaceFolders': [{'uri': uri_of(self.root), 'name': 'workspace'}],
            'initializationOptions': {'declarationFiles': [uri_of(declaration)]}})
        uri = uri_of(self.root / 'explicit.tjs')
        client.open(uri, 'explicitOnly;')
        client.diagnostics()
        location = client.request(2, 'textDocument/definition', {
            'textDocument': {'uri': uri}, 'position': {'line': 0, 'character': 3}})
        self.assertEqual(location['uri'], uri_of(declaration))
        client.close()

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

    def test_disk_workspace_accepts_utf8_localhost_and_encoded_drive(self):
        other = self.root / "子目录 %25" / "跨文件 %25.tjs"
        other.parent.mkdir()
        other.write_text("function diskOnlyHelper() {}\n", encoding="utf-8")
        uri = uri_of(self.root).replace("file:///", "file://localhost/")
        if sys.platform == "win32":
            uri = uri.replace(f"/{self.root.drive}/", f"/{self.root.drive[0]}%3a/", 1)
        self.client.send({"jsonrpc": "2.0", "method": "workspace/didChangeWorkspaceFolders",
                          "params": {"event": {"removed": [{"uri": uri_of(self.root)}],
                                               "added": []}}})
        self.client.send({"jsonrpc": "2.0", "method": "workspace/didChangeWorkspaceFolders",
                          "params": {"event": {"removed": [],
                                               "added": [{"uri": uri, "name": "encoded"}]}}})
        main = self.open_document("lookup.tjs", "diskOnlyHelper;")
        location = self.client.request(30, "textDocument/definition", {
            "textDocument": {"uri": main}, "position": {"line": 0, "character": 5}})
        self.assertEqual(location["uri"], uri_of(other))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *rest])
