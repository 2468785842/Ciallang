"""Protocol integration test: run with --cial PATH; needs no third-party modules."""
import argparse
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from cial_lsp import source_position, line_prefix, utf16

parser = argparse.ArgumentParser()
parser.add_argument("--cial", required=True)
args, rest = parser.parse_known_args()


class ProtocolTest(unittest.TestCase):
    def setUp(self):
        self.process = subprocess.Popen([sys.executable, str(pathlib.Path(__file__).with_name("cial_lsp.py")),
                                         "--cial", args.cial], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)

    def tearDown(self):
        if self.process.poll() is None:
            self.process.terminate()
        self.process.communicate(timeout=5)

    def send(self, method, params=None, identifier=None):
        value = {"jsonrpc": "2.0", "method": method, "params": params or {}}
        if identifier is not None:
            value["id"] = identifier
        data = json.dumps(value).encode("utf-8")
        self.process.stdin.write(b"Content-Length: " + str(len(data)).encode("ascii") + b"\r\n\r\n" + data)
        self.process.stdin.flush()

    def receive(self):
        headers = {}
        while True:
            line = self.process.stdout.readline()
            self.assertTrue(line, "server exited before response")
            if line == b"\r\n":
                break
            key, value = line.decode("ascii").split(":", 1)
            headers[key.lower()] = value.strip()
        return json.loads(self.process.stdout.read(int(headers["content-length"])))

    def initialize(self, params=None):
        self.send("initialize", params, identifier=1)
        capabilities = self.receive()["result"]["capabilities"]
        self.assertEqual(capabilities["positionEncoding"], "utf-16")
        self.assertEqual(capabilities["textDocumentSync"]["change"], 1)
        self.assertTrue(capabilities["workspaceSymbolProvider"])
        self.assertTrue(capabilities["definitionProvider"])
        self.assertTrue(capabilities["workspace"]["workspaceFolders"]["supported"])
        self.send("initialized")

    def symbols(self, query=""):
        self.send("workspace/symbol", {"query": query}, 20)
        return self.receive()["result"]

    def open_document(self, uri, text, version=1):
        self.send("textDocument/didOpen", {"textDocument": {"uri": uri, "text": text, "version": version}})
        self.receive()

    def definition(self, uri, text, needle, offset=0):
        cursor = text.index(needle) + offset
        before = text[:cursor]
        position = {"line": before.count("\n"), "character": utf16(before.rsplit("\n", 1)[-1])}
        self.send("textDocument/definition", {"textDocument": {"uri": uri}, "position": position}, 21)
        return self.receive()["result"]

    def assert_definition(self, location, uri, text, declaration, name):
        self.assertIsNotNone(location)
        before = text[:text.index(declaration)]
        start = {"line": before.count("\n"), "character": utf16(before.rsplit("\n", 1)[-1])}
        self.assertEqual(location, {"uri": uri, "range": {"start": start,
                         "end": {"line": start["line"], "character": start["character"] + utf16(name)}}})

    def test_definitions_respect_blocks_parameters_and_function_boundaries(self):
        self.initialize()
        uri = "file:///definitions.tjs"
        text = '''var value = 0;
function outer(value:int, other = value) {
    value; // parameter
    { var value = value + 1; value; } // inner
    value; // restored
    target();
    function target(arg) { return arg; }
    { var local; local; }
    local; // out of scope
    function nested() { return other; }
}
value; // global
'''
        self.open_document(uri, text)
        for reference in ("value; // parameter", "value + 1", "value; // restored"):
            self.assert_definition(self.definition(uri, text, reference), uri, text, "value:int", "value")
        self.assert_definition(self.definition(uri, text, "value; } // inner"), uri, text,
                               "value = value + 1", "value")
        self.assert_definition(self.definition(uri, text, "target();"), uri, text, "target(arg)", "target")
        self.assert_definition(self.definition(uri, text, "arg;"), uri, text, "arg)", "arg")
        self.assert_definition(self.definition(uri, text, "local; }"), uri, text, "local; local", "local")
        self.assertIsNone(self.definition(uri, text, "local; // out"))
        self.assertIsNone(self.definition(uri, text, "other;"))  # TJS does not capture outer locals.
        self.assert_definition(self.definition(uri, text, "value; // global"), uri, text, "value = 0", "value")

    def test_definitions_for_catch_and_dynamic_member_contexts(self):
        self.initialize()
        uri = "file:///control-definitions.tjs"
        text = '''var index = 99;
for(var index = 0; index < 2; index++) { index; }
index; // after loop
try { throw 1; } catch(error) { error; }
error; // after catch
var shared = 1;
with(global) { shared; }
class Example { function method(arg) { arg; shared; } }
'''
        self.open_document(uri, text)
        self.assert_definition(self.definition(uri, text, "index; }"), uri, text, "index = 0", "index")
        self.assert_definition(self.definition(uri, text, "index; // after"), uri, text, "index = 99", "index")
        self.assert_definition(self.definition(uri, text, "error; }"), uri, text, "error)", "error")
        self.assertIsNone(self.definition(uri, text, "error; // after"))
        self.assertIsNone(self.definition(uri, text, "shared; }"))
        self.assertIsNone(self.definition(uri, text, "shared; } }"))
        self.assert_definition(self.definition(uri, text, "arg;"), uri, text, "arg)", "arg")

    def test_definitions_only_use_unique_top_level_workspace_declarations(self):
        with tempfile.TemporaryDirectory(prefix="cial definition %20 中文 ") as directory:
            root = pathlib.Path(directory)
            library = root / "library.tjs"
            library_text = '"😀"; function Shared() {}\nfunction Owner() { var Hidden; }\nvar First=1, Tail=2;'
            library.write_text(library_text, encoding="utf-8")
            uri = (root / "main.tjs").as_uri()
            text = 'Shared(); Tail; Hidden; shared(); var receiver; receiver.Shared(); "Shared"; // Shared\nRenamed();'
            self.initialize({"rootUri": root.as_uri()})
            self.open_document(uri, text)
            self.assert_definition(self.definition(uri, text, "Shared();"), library.as_uri(), library_text,
                                   "Shared()", "Shared")
            self.assert_definition(self.definition(uri, text, "Tail;"), library.as_uri(), library_text, "Tail=2", "Tail")
            for reference, offset in (("Hidden;", 0), ("shared();", 0), ("receiver.Shared", len("receiver.")),
                                      ('"Shared"', 1), ("// Shared", 3)):
                self.assertIsNone(self.definition(uri, text, reference, offset))
            duplicate = root / "duplicate.tjs"
            duplicate.write_text("function Shared() {}", encoding="utf-8")
            self.assertIsNone(self.definition(uri, text, "Shared();"))
            duplicate.unlink()
            self.open_document(library.as_uri(), "function Renamed() {}")
            self.assertIsNone(self.definition(uri, text, "Shared();"))
            self.assert_definition(self.definition(uri, text, "Renamed();"), library.as_uri(),
                                   "function Renamed() {}", "Renamed()", "Renamed")

    def test_definitions_decline_incomplete_and_preprocessed_source(self):
        self.initialize()
        uri = "file:///unsupported-definitions.tjs"
        for version, text in enumerate(("var value; function broken( { value;", "@set(flag=1)\nvar value; value;"), 1):
            self.open_document(uri, text, version)
            self.assertIsNone(self.definition(uri, text, "value;", len("value;") - 2))

    def test_workspace_symbols_disk_buffers_and_folder_changes(self):
        with tempfile.TemporaryDirectory(prefix="cial workspace %20 空间 ") as directory:
            workspace = pathlib.Path(directory) / "first"
            second = pathlib.Path(directory) / "second"
            workspace.mkdir()
            second.mkdir()
            source = workspace / "declarations.tjs"
            text = '"😀"; function Find中文() {}\n'
            source.write_text(text, encoding="utf-8")
            (workspace / "comments.tjs").write_text(
                '// function FindComment() {}\n/* class FindBlock {} */\n'
                'var text = "function FindString() {}";\nvar regex = /function FindRegex()/;\n',
                encoding="utf-8")
            (workspace / "build").mkdir()
            (workspace / "build/ignored.tjs").write_text("function FindBuild() {}", encoding="utf-8")
            (workspace / "invalid.tjs").write_bytes(b"function FindInvalid() {}\xff")
            (workspace / "large.tjs").write_text("function FindLarge() {}" + " " * 2_000_000, encoding="utf-8")
            (second / "extra.tjs").write_text("class FindSecond {}", encoding="utf-8")
            self.initialize({"workspaceFolders": [{"uri": workspace.as_uri(), "name": "first"}]})
            symbols = self.symbols("find")
            self.assertEqual([symbol["name"] for symbol in symbols], ["Find中文"])
            self.assertEqual(symbols[0]["location"]["uri"], source.as_uri())
            self.assertEqual(symbols[0]["location"]["range"]["start"],
                             {"line": 0, "character": utf16(text[:text.index("Find中文")])})

            # An unopened file is refreshed directly from disk, without a watcher dependency.
            source.write_text("function FindDiskChanged() {}", encoding="utf-8")
            self.assertEqual([symbol["name"] for symbol in self.symbols("find")], ["FindDiskChanged"])
            self.send("textDocument/didOpen", {"textDocument": {
                "uri": source.as_uri(), "version": 1, "text": "function FindBuffer() {}"}})
            self.receive()
            self.assertEqual([symbol["name"] for symbol in self.symbols("find")], ["FindBuffer"])
            self.send("textDocument/didChange", {"textDocument": {"uri": source.as_uri(), "version": 2},
                                                "contentChanges": [{"text": "function FindEdited() {}"}]})
            self.receive()
            self.assertEqual([symbol["name"] for symbol in self.symbols("find")], ["FindEdited"])
            self.send("textDocument/didClose", {"textDocument": {"uri": source.as_uri()}})
            self.receive()
            self.assertEqual([symbol["name"] for symbol in self.symbols("find")], ["FindDiskChanged"])
            source.unlink()
            self.assertEqual(self.symbols("find"), [])

            self.send("workspace/didChangeWorkspaceFolders", {"event": {
                "added": [{"uri": second.as_uri(), "name": "second"}],
                "removed": [{"uri": workspace.as_uri(), "name": "first"}]}})
            self.assertEqual([symbol["name"] for symbol in self.symbols("find")], ["FindSecond"])

    def test_workspace_symbols_root_uri_and_result_limit(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = pathlib.Path(directory)
            (workspace / "many.tjs").write_text(
                "\n".join("var Find%03d = 0;" % index for index in range(205)), encoding="utf-8")
            self.initialize({"rootUri": workspace.as_uri()})
            symbols = self.symbols("find")
            self.assertEqual(len(symbols), 200)
            self.assertEqual(symbols[0]["name"], "Find000")
            self.assertEqual(symbols[-1]["name"], "Find199")
            self.assertEqual([symbol["name"] for symbol in self.symbols("FIND204")], ["Find204"])

    def test_workspace_symbols_without_roots_only_search_open_files(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / "outside.tjs"
            source.write_text("function FindDisk() {}", encoding="utf-8")
            self.initialize()
            self.assertEqual(self.symbols("find"), [])
            self.send("textDocument/didOpen", {"textDocument": {
                "uri": source.as_uri(), "version": 1, "text": "function FindOpen() {}"}})
            self.receive()
            self.assertEqual([symbol["name"] for symbol in self.symbols("find")], ["FindOpen"])
            self.send("textDocument/didClose", {"textDocument": {"uri": source.as_uri()}})
            self.receive()
            self.assertEqual(self.symbols("find"), [])

    def test_diagnostics_fix_and_close(self):
        self.initialize()
        uri = "file:///protocol-test.tjs"
        self.send("textDocument/didOpen", {"textDocument": {"uri": uri, "version": 1, "text": "var x = ;"}})
        report = self.receive()["params"]
        self.assertEqual(report["version"], 1)
        self.assertTrue(report["diagnostics"])
        self.assertNotIn("unavailable", report["diagnostics"][0]["message"])
        self.send("textDocument/didChange", {"textDocument": {"uri": uri, "version": 2},
                                            "contentChanges": [{"text": "var x = 42; throw 7;"}]})
        self.assertEqual(self.receive()["params"]["diagnostics"], [])  # throw 不得被执行
        self.send("textDocument/didClose", {"textDocument": {"uri": uri}})
        self.assertEqual(self.receive()["params"]["diagnostics"], [])

    def test_completion_hover_symbols_and_lifecycle(self):
        self.initialize()
        uri = "file:///completion.tjs"
        self.send("textDocument/didOpen", {"textDocument": {"uri": uri, "version": 1,
                                                          "text": "function foobar() {}\nvar value = 1;"}})
        self.receive()
        self.send("textDocument/completion", {"textDocument": {"uri": uri}, "position": {"line": 1, "character": 0}}, 2)
        labels = {x["label"] for x in self.receive()["result"]}
        self.assertIn("foobar", labels)
        self.assertIn("function", labels)
        self.send("textDocument/hover", {"textDocument": {"uri": uri}, "position": {"line": 0, "character": 2}}, 3)
        self.assertIn("function", self.receive()["result"]["contents"]["value"])
        self.send("textDocument/documentSymbol", {"textDocument": {"uri": uri}}, 4)
        self.assertEqual({x["name"] for x in self.receive()["result"]}, {"foobar", "value"})
        self.send("unknown", identifier=5)
        self.assertEqual(self.receive()["error"]["code"], -32601)
        self.send("shutdown", identifier=6)
        self.assertIsNone(self.receive()["result"])
        self.send("exit")
        self.assertEqual(self.process.wait(timeout=5), 0)

    def test_utf8_columns_and_utf16_cursor(self):
        text = "var 中文 = '😀';"
        prefix = "var 中文 = '😀"
        native_column = len(prefix.encode("utf-8"))
        self.assertEqual(source_position(text, 0, native_column)["character"], len(prefix) + 1)
        self.assertEqual(line_prefix(text, {"line": 0, "character": len(prefix) + 1}), prefix)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *rest])
