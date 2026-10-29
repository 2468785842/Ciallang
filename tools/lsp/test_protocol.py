"""Protocol integration test: run with --cial PATH; needs no third-party modules."""
import argparse
import json
import pathlib
import subprocess
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from cial_lsp import source_position, line_prefix

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

    def initialize(self):
        self.send("initialize", identifier=1)
        capabilities = self.receive()["result"]["capabilities"]
        self.assertEqual(capabilities["positionEncoding"], "utf-16")
        self.assertEqual(capabilities["textDocumentSync"]["change"], 1)
        self.send("initialized")

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
