"""Exercise the CLI as a process, including output contracts and REPL recovery."""
import argparse
import json
import os
import pathlib
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument("--cial", required=True)
parser.add_argument("--expected-version")
args, rest = parser.parse_known_args()
CIAL = str(pathlib.Path(args.cial).resolve())


class CliTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)
        self.script = self.root / "script.tjs"
        self.script.write_text('println(42);', encoding="utf-8")

    def run_cial(self, *arguments, input=None):
        environment = dict(os.environ)
        environment.pop("CIAL_DUMP", None)
        environment.pop("CIAL_TJSBC_DUMP", None)
        return subprocess.run([CIAL, *map(str, arguments)], input=input, text=True,
                              encoding="utf-8", capture_output=True, timeout=15,
                              cwd=self.root, env=environment)

    def test_help_version_and_arguments(self):
        self.assertEqual(self.run_cial("--help").returncode, 0)
        self.assertIn("--repl", self.run_cial("--help").stdout)
        version = self.run_cial("--version")
        self.assertEqual(version.returncode, 0)
        if args.expected_version:
            self.assertEqual(version.stdout.strip(), args.expected_version)
        for flags in [("--unknown",), ("--tjsbc",), (self.script, "extra"),
                      ("--jit", "--trace-jit", self.script),
                      ("--check-json", "--dump-cfg", self.script),
                      ("--repl", self.script), ("--check-json", "--time", self.script),
                      ("--trace-execution", self.script)]:
            with self.subTest(flags=flags):
                process = self.run_cial(*flags)
                self.assertEqual(process.returncode, 2, process.stderr)
                self.assertEqual(process.stdout, "")

    def test_script_output_and_timing(self):
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout, "42\n")
        self.assertEqual(process.stderr, "")
        process = self.run_cial("--time", self.script)
        self.assertEqual(process.stdout, "42\n")
        self.assertIn("Time taken", process.stderr)

    def test_errors_are_failures_and_json_does_not_execute(self):
        for source in ["var ;", "for(", "var x = ;", 'throw "boom";']:
            with self.subTest(source=source):
                self.script.write_text(source, encoding="utf-8")
                process = self.run_cial(self.script)
                self.assertEqual(process.returncode, 1)
                self.assertIn("cial:", process.stderr)
        self.script.write_text('println(42); throw "must not execute";', encoding="utf-8")
        process = self.run_cial("--check-json", self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertTrue(json.loads(process.stdout)["ok"])
        self.assertEqual(process.stderr, "")
        self.script.write_text("var x = ;", encoding="utf-8")
        process = self.run_cial("--check-json", self.script)
        self.assertEqual(process.returncode, 1)
        diagnostic = json.loads(process.stdout)
        self.assertFalse(diagnostic["ok"])
        self.assertTrue(diagnostic["diagnostics"])
        self.assertEqual(diagnostic["columnEncoding"], "utf-8")

    def test_runtime_error_rendered_with_source_identity(self):
        # VM 只传播异常值；文件与位置由顶层附加，message 内不再拼行号
        self.script.write_text('function f(){ throw "inner"; }\nf();\n', encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 1)
        self.assertEqual(process.stderr.splitlines()[0],
                         'cial: error: %s:1:15: "inner"' % self.script)  # throw 语句位置
        # 打印时按文件身份重读渲染摘录（linecache 式），指示符指向 throw
        excerpt = process.stderr.splitlines()
        self.assertTrue(any(line.rstrip().endswith('throw "inner"; }') for line in excerpt))
        self.assertEqual(excerpt[2], " " * (7 + 14) + "^")
        # 被捕获的异常 message 保持纯净（println 输出字符串带引号）
        self.script.write_text('try { var v = void; v.x; } catch(e) { println(e.message); }\n',
                               encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout.splitlines()[0], '"cannot access member of void"')
        self.assertNotIn("line", process.stdout)
        # 连续执行不残留上一轮故障位置
        self.script.write_text('try { throw 1; } catch(e) {}\nthrow "second";\n', encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.stderr.splitlines()[0], 'cial: error: %s:2:1: "second"' % self.script)
        # stdin 逻辑名读不回源码：退化为单行，不伪造摘录
        process = self.run_cial("-", input='throw "s";\n')
        self.assertEqual(process.returncode, 1)
        self.assertEqual(process.stderr, 'cial: error: <stdin>:1:1: "s"\n')
        self.assertEqual(self.run_cial(self.root / "missing.tjs").returncode, 1)
        self.assertFalse(json.loads(self.run_cial("--check-json", self.root / "missing.tjs").stdout)["ok"])

    def test_empty_script_and_graph_modes(self):
        self.script.write_text("// empty\n", encoding="utf-8")
        self.assertEqual(self.run_cial(self.script).returncode, 0)
        self.script.write_text('function f(x) { return x + 1; } println(f(41));', encoding="utf-8")
        for flag in ["--dump-cfg", "--dump-ssa"]:
            process = self.run_cial(flag, self.script)
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertIn("top-level", process.stdout)
            self.assertNotIn("\n42\n", process.stdout)

    def test_trace_survives_runtime_failure(self):
        self.script.write_text('println(42); throw "boom";', encoding="utf-8")
        output = self.root / "trace.dot"
        process = self.run_cial("--trace-execution", self.script, output)
        self.assertEqual(process.returncode, 1)
        self.assertEqual(process.stdout, "42\n")
        self.assertIn("digraph", output.read_text(encoding="utf-8"))
        original = self.script.read_text(encoding="utf-8")
        process = self.run_cial("--trace-execution", self.script, self.script)
        self.assertEqual(process.returncode, 1)
        self.assertEqual(self.script.read_text(encoding="utf-8"), original)

    def test_repl_globals_multiline_and_regex(self):
        process = self.run_cial("--repl", input='''var x = 40;
x + 2
function add(v) {
return v + 1;
}
add(x + 1)
var a = [
1, 2, 3
];
a.count
/[{}]/.test("{")
/* open {
/* nested */ comment */
40 + \\
2
:quit
''')
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout.splitlines(), ["42", "42", "3", "1", "42"])
        self.assertEqual(process.stderr, "")

    def test_repl_error_recovery_and_abort(self):
        process = self.run_cial(input='''var x = 40;
var broken = ;
throw "boom";
x + 2
function discard() {
:abort
x + 3
:quit
''')
        self.assertEqual(process.returncode, 1)
        self.assertEqual(process.stdout.splitlines(), ["42", "43"])
        self.assertIn("boom", process.stderr)
        self.assertEqual(self.run_cial("--repl", input="function f() {\n").returncode, 1)

    def test_repl_requires_complete_expression_and_evaluates_once(self):
        process = self.run_cial("--repl", input='''var x = 0;
x++
x
1 2
x
:quit
''')
        self.assertEqual(process.returncode, 1)
        self.assertEqual(process.stdout.splitlines(), ["0", "1", "1"])

    def test_repl_multiline_string_and_preprocessor_state(self):
        process = self.run_cial("--repl", input='''var text = "a
b";
text.length
@set(flag=1)
@if(flag)
var selected = 42;
@endif
selected
:quit
''')
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout.splitlines(), ["3", "42"])

    def test_file_names_with_spaces_and_option_prefix(self):
        for name in ["a script.tjs", "--script.tjs"]:
            path = self.root / name
            path.write_text("println(42);", encoding="utf-8")
            process = self.run_cial("--", name)
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(process.stdout, "42\n")

    def test_repl_many_compilations_preserve_globals_across_gc(self):
        source = "var answer = 42;\n" + "function f() { return answer; }\nf()\n" * 300 + ":quit\n"
        process = self.run_cial("--repl", input=source)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout.splitlines(), ["42"] * 300)

    def test_unterminated_literals_and_nested_comment(self):
        for source in [
            'var x = /abc', '/* unterminated', 'var x = "abc',
            '@set(x=1/0)', '@set(x=1%0)', 'class 0 {}',
            'function 0 {}', 'property 0 {}', 'property foo {',
        ]:
            with self.subTest(source=source):
                self.script.write_text(source, encoding="utf-8")
                process = self.run_cial("--check-json", self.script)
                self.assertEqual(process.returncode, 1)
                self.assertFalse(json.loads(process.stdout)["ok"])

    def test_json_schema_v2_structure(self):
        self.script.write_text("var b = ;", encoding="utf-8")
        report = json.loads(self.run_cial("--check-json", self.script).stdout)
        self.assertEqual(report["schema"], 2)
        self.assertEqual(report["columnEncoding"], "utf-8")
        self.assertEqual(report["source"], str(self.script))
        diagnostic = report["diagnostics"][0]
        self.assertEqual(diagnostic["code"], "parse.invalid-expression")
        self.assertEqual(diagnostic["line"], 0)
        self.assertEqual(diagnostic["column"], 8)
        self.assertEqual(diagnostic["endColumn"], 9)  # 半开：单字符 token 宽 1
        self.assertEqual(diagnostic["range"], {"begin": 8, "end": 9})
        self.assertNotIn("(", diagnostic["message"])  # 纯文本，无 (file@l:c) 前缀
        self.assertNotIn(self.script.name, diagnostic["message"])

    def test_json_schema_v1_is_derived_legacy_shape(self):
        self.script.write_text("var b = ;", encoding="utf-8")
        report = json.loads(self.run_cial("--check-json", "--json-schema", "1", self.script).stdout)
        self.assertNotIn("schema", report)
        diagnostic = report["diagnostics"][0]
        self.assertEqual(diagnostic["endColumn"], 8)  # 旧包含式由 v2 派生
        self.assertNotIn("code", diagnostic)

    def test_json_schema_rejects_unknown_versions(self):
        for version in ["0", "3", "x"]:
            with self.subTest(version=version):
                process = self.run_cial("--check-json", "--json-schema", version, self.script)
                self.assertEqual(process.returncode, 2)
                self.assertIn("json-schema", process.stderr)

    def test_stdin_input_keeps_source_identity(self):
        process = self.run_cial("--check-json", "--source-name", "untitled:tjs-1", "-",
                                input='var 中文 = ;\n')
        report = json.loads(process.stdout)
        self.assertFalse(report["ok"])
        self.assertEqual(report["source"], "untitled:tjs-1")
        diagnostic = report["diagnostics"][0]
        self.assertEqual(diagnostic["source"], "untitled:tjs-1")
        self.assertEqual(diagnostic["column"], 13)  # 字节列：var␣中文(6B)␣=␣;
        self.assertEqual(diagnostic["range"], {"begin": 13, "end": 14})
        # 未保存内容的合法输入同样可走 stdin
        ok = json.loads(self.run_cial("--check-json", "-", input="println(42);").stdout)
        self.assertTrue(ok["ok"])
        self.assertEqual(ok["source"], "<stdin>")

    def test_source_name_requires_stdin(self):
        process = self.run_cial("--check-json", "--source-name", "x", self.script)
        self.assertEqual(process.returncode, 2)
        self.assertIn("--source-name", process.stderr)

    def test_text_diagnostics_render_location_and_excerpt(self):
        self.script.write_text('var a = "中文"; var b = ;\n', encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 1)
        self.assertEqual(process.stderr.splitlines()[0],
                         "cial: error[parse.invalid-expression]: %s:1:27: unable prefix parse for token ';'." % self.script)
        excerpt = process.stderr.splitlines()
        self.assertTrue(any(line.rstrip().endswith("var b = ;") for line in excerpt))
        # 指示符按显示宽度对齐：栏 7 列 + 前缀 16 显示列（中文各 2 列）+ "var b = " 8 列
        caret = excerpt[2]
        self.assertEqual(caret.index("^"), 7 + 24)
        self.assertTrue(caret.endswith("^"))
        self.assertNotIn("\x1b[", process.stderr)  # 管道输出不带 ANSI

    def test_no_color_flag(self):
        self.script.write_text("var ;", encoding="utf-8")
        process = self.run_cial("--no-color", self.script)
        self.assertEqual(process.returncode, 1)
        self.assertNotIn("\x1b[", process.stderr)

        self.script.write_text('/* **/ println(42);', encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout, "42\n")

    def test_bytecode_corpus_and_invalid_file(self):
        corpus = pathlib.Path(__file__).resolve().parent / "test_files" / "tjsbc"
        for path in sorted(corpus.glob("*.tjsbc")):
            with self.subTest(path=path.name):
                process = self.run_cial("--tjsbc", path)
                self.assertEqual(process.returncode, 0, process.stderr)
                self.assertEqual(process.stderr, "")
        invalid = self.root / "invalid.tjsbc"
        invalid.write_bytes(b"not TJS bytecode")
        process = self.run_cial("--tjsbc", invalid)
        self.assertEqual(process.returncode, 1)
        self.assertIn("cial:", process.stderr)

    def test_execution_options_compose_and_jit_is_optional(self):
        process = self.run_cial("--tail-recursion", "--time", self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout, "42\n")
        self.assertIn("[tail]", process.stderr)
        self.assertIn("Time taken", process.stderr)
        process = self.run_cial("--jit", self.script)
        if process.returncode == 0:
            self.assertEqual(process.stdout, "42\n")
            self.assertIn("[jit]", process.stderr)
        else:
            self.assertEqual(process.returncode, 1)
            self.assertIn("JIT backend not enabled", process.stderr)


if __name__ == "__main__":
    unittest.main(argv=[__file__, *rest])
