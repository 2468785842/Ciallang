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
        self.assertEqual(self.run_cial("--version").returncode, 0)
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
