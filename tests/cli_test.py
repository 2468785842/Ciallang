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
        # 摘录使用编译时快照，指示符指向 throw
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
        # 虚拟输入也保留正文
        process = self.run_cial("-", input='throw "s";\n')
        self.assertEqual(process.returncode, 1)
        self.assertEqual(process.stderr.splitlines()[0], 'cial: error: <stdin>:1:1: "s"')
        self.assertIn('throw "s";', process.stderr)
        self.assertEqual(self.run_cial(self.root / "missing.tjs").returncode, 1)
        self.assertFalse(json.loads(self.run_cial("--check-json", self.root / "missing.tjs").stdout)["ok"])

    def test_runtime_excerpt_uses_compiled_content_for_named_stdin(self):
        self.script.write_text('// unrelated disk contents', encoding="utf-8")
        process = self.run_cial("--source-name", self.script, "-", input='throw "snapshot";\n')
        self.assertEqual(process.returncode, 1)
        self.assertIn('throw "snapshot";', process.stderr)
        self.assertNotIn('unrelated disk contents', process.stderr)

    def test_runtime_excerpt_survives_script_overwriting_its_file(self):
        path = json.dumps(self.script.as_posix())
        self.script.write_text('["// replaced on disk"].save(%s);\nthrow "original";\n' % path,
                               encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 1)
        self.assertIn('replaced on disk', self.script.read_text(encoding="utf-8-sig"))
        self.assertIn('throw "original";', process.stderr)
        self.assertIn('%s:2:1:' % self.script, process.stderr)

    def test_repl_history_keeps_original_function_body(self):
        process = self.run_cial("--repl", input='function saved() { throw "history"; }\n'
                               'var later = 42;\nsaved();\nprintln(42);\n')
        self.assertEqual(process.returncode, 1)
        self.assertIn('function saved() { throw "history"; }', process.stderr)
        self.assertNotIn('var later = 42;', process.stderr)
        self.assertIn('42', process.stdout)

    def test_eval_function_keeps_source_after_eval_returns(self):
        self.script.write_text('var delayed = \'function() { throw "eval-body"; }\'!;\ndelayed();\n',
                               encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 1)
        self.assertIn('<eval>:1:', process.stderr)
        self.assertIn('function() { throw "eval-body"; }', process.stderr)
        self.assertNotIn("var delayed =", process.stderr)

    def test_runtime_stack_lists_fault_and_caller_positions(self):
        self.script.write_text('function leaf() {\nthrow "inner";\n}\n'
                               'function middle() {\nleaf();\n}\nmiddle();\nvar later = 42;\n', encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 1)
        frames = [line for line in process.stderr.splitlines() if line.startswith("  at ")]
        self.assertEqual(frames, ['  at leaf (%s:2:1)' % self.script,
                                  '  at middle (%s:5:1)' % self.script,
                                  '  at <script> (%s:7:1)' % self.script])

    def test_runtime_stack_preserves_nested_storage_origin(self):
        child = self.root / "child.tjs"
        child.write_text('function leaf() { throw "storage"; }\nleaf();\n', encoding="utf-8")
        self.script.write_text('function wrapper() {\nScripts.execStorage("child.tjs");\n}\nwrapper();\n',
                               encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 1)
        self.assertIn('throw "storage";', process.stderr)
        frames = [line for line in process.stderr.splitlines() if line.startswith("  at ")]
        self.assertEqual(len(frames), 5)
        self.assertIn('at leaf (', frames[0])
        self.assertIn('child.tjs:1:', frames[0])
        self.assertIn('child.tjs:2:1)', frames[1])
        self.assertIn('<native boundary>', frames[2])
        self.assertEqual(frames[3], '  at wrapper (%s:2:1)' % self.script)
        self.assertEqual(frames[4], '  at <script> (%s:4:1)' % self.script)

    def test_runtime_stack_tracks_property_constructor_and_eval_call_sites(self):
        for source, caller_column in [
                ('property p { getter { throw "getter"; } }\nvar v = p;\n', 5),
                ('property p { setter(v) { throw "setter"; } }\np = 1;\n', 1),
                ('class C { function C() { throw "constructor"; } }\nnew C();\n', 1),
                ('var expression = "1 / 0";\nexpression!;\n', 11)]:
            with self.subTest(source=source):
                self.script.write_text(source, encoding="utf-8")
                process = self.run_cial(self.script)
                self.assertEqual(process.returncode, 1)
                self.assertIn('  at <script> (%s:2:%s)' % (self.script, caller_column), process.stderr)
                self.assertGreaterEqual(process.stderr.count("  at "), 2)
        self.assertIn('<eval>:1:', process.stderr)

    def test_caught_callback_exception_does_not_print_a_stack(self):
        self.script.write_text('function compare(a, b) { throw 37; }\n'
                               'try { [2, 1].sort(compare); } catch(e) { println(e); }\n', encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, "")
        self.assertEqual(process.stdout, "37\n")

    def test_caught_getter_exception_resumes_caller_once(self):
        self.script.write_text('property p { getter { throw 73; } }\n'
                               'try { var v = p; } catch(e) { println(e); }\nprintln(42);\n', encoding="utf-8")
        process = self.run_cial(self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, "")
        self.assertEqual(process.stdout, "73\n42\n")

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

    def test_repl_declaration_without_semicolon(self):
        process = self.run_cial("--repl", input='''var x = 40
x + 2
const c = 7
c
var flagged = 1 // trailing comment
flagged
do { x = x + 1; } while(x < 44)
x
:quit
''')
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout.splitlines(), ["42", "7", "1", "44"])
        self.assertEqual(process.stderr, "")

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

    def test_decompile_options_and_strict_mode(self):
        # 互斥与从属校验
        for flags in [("--decompile", "--tjsbc", self.script),
                      ("--decompile", "--time", self.script),
                      ("--view", self.script), ("--map", "m.json", self.script),
                      ("--source", "orig.tjs", self.script)]:
            with self.subTest(flags=flags):
                process = self.run_cial(*flags)
                self.assertEqual(process.returncode, 2, process.stderr)
                self.assertEqual(process.stdout, "")
        # 自产路径：直线表达式严格模式恢复成功（M2），退出码 0、产出可重编译文本
        process = self.run_cial("--decompile", self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("println(42);", process.stdout)
        self.assertNotIn("//   @", process.stdout)
        # 语句结构未落地（M3）：严格模式 Unsupported，退出码 2、不产出文本、逐单元诊断
        structured = self.root / "structured.tjs"
        structured.write_text("var a = 1;\nif (a) { println(a); }\n", encoding="utf-8")
        process = self.run_cial("--decompile", structured)
        self.assertEqual(process.returncode, 2, process.stderr)
        self.assertEqual(process.stdout, "")
        self.assertIn("decompile unsupported", process.stderr)
        # 诊断视图：输出含行号与 TAC 反汇编，头部标注不可重编译，恢复结果以注释附加
        process = self.run_cial("--decompile", "--view", self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("not recompilable", process.stdout)
        self.assertIn("@0", process.stdout)
        self.assertIn("//   >> println(42);", process.stdout)
        # 行表 JSON：状态与行坐标
        mapPath = self.root / "map.json"
        process = self.run_cial("--decompile", "--view", "--map", mapPath, self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        payload = json.loads(mapPath.read_text(encoding="utf-8"))
        self.assertEqual(payload["status"], "unsupported")
        self.assertTrue(payload["units"])
        # TJS2 路径：语料反汇编视图 + --source 行号换算；非法输入拒绝
        corpus = pathlib.Path(__file__).resolve().parent / "test_files" / "tjsbc"
        sample = corpus / "s6_class.tjsbc"
        process = self.run_cial("--decompile", "--view", sample)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("context", process.stdout)
        source = corpus / "s6_class.tjs"
        process = self.run_cial("--decompile", "--view", "--source", source, sample)
        self.assertEqual(process.returncode, 0, process.stderr)
        garbage = self.root / "garbage.bin"
        garbage.write_bytes(b"\x01\x02\x03\x04 not a script or bytecode")
        process = self.run_cial("--decompile", garbage)
        self.assertNotEqual(process.returncode, 0)

    def test_decompile_outputdebug_source_lines(self):
        # 真实 outputdebug 产物（manifest 固定参考二进制/导出选项/样本哈希）经
        # --source 做偏移→行号换算的端到端；合成用例在 decompile_test.cpp
        corpus = pathlib.Path(__file__).resolve().parent / "test_files" / "tjsbc" / "debug"
        manifest = json.loads((corpus / "manifest.json").read_text(encoding="utf-8"))
        import hashlib

        for sample in manifest["samples"]:
            source = corpus / f"{sample['name']}.tjs"
            bytecode = corpus / f"{sample['name']}.tjsbc"
            map_path = self.root / f"{sample['name']}_map.json"
            with self.subTest(sample=sample["name"]):
                for name, field in [("source", source), ("bytecode", bytecode)]:
                    digest = hashlib.sha256(field.read_bytes()).hexdigest()
                    self.assertEqual(digest, sample[f"{name}Sha256"], name)
                process = self.run_cial("--decompile", "--view", "--source", source,
                                        "--map", map_path, bytecode)
                self.assertEqual(process.returncode, 0, process.stderr)
                payload = json.loads(map_path.read_text(encoding="utf-8"))
                self.assertEqual(payload["status"], "unsupported")
                # 行号落在源文件行数内（0 基；末条目可指向末尾之后，由换算收敛）
                line_count = source.read_text(encoding="utf-8").count("\n") + 1
                units = {unit["name"]: [row.get("srcLine") for row in unit["lines"]]
                         for unit in payload["units"]}
                for name, lines in units.items():
                    for line in lines:
                        if line is not None:
                            self.assertGreaterEqual(line, 0, name)
                            self.assertLess(line, line_count, f"{sample['name']}/{name}")
                for name, expected in sample["expectedLines"].items():
                    self.assertIn(name, units, f"{sample['name']}/{name}")
                    for line in expected:
                        self.assertIn(line, units[name], f"{sample['name']}/{name}")
        # 无 outputdebug 表的既有语料不携带 srcLine
        plain_map = self.root / "plain_map.json"
        process = self.run_cial("--decompile", "--view", "--source", corpus.parent / "s6_class.tjs",
                                "--map", plain_map, corpus.parent / "s6_class.tjsbc")
        self.assertEqual(process.returncode, 0, process.stderr)
        payload = json.loads(plain_map.read_text(encoding="utf-8"))
        self.assertFalse(any(row.get("srcLine") is not None
                             for unit in payload["units"] for row in unit["lines"]))

    def test_decompile_expression_roundtrip(self):
        # 环①（方案 §7）：直线表达式程序 源码执行 vs 严格反编译再编译执行，逐形态
        # 对拍可观察输出。覆盖字面量/算术/位/比较/逻辑/一元/成员/下标/调用/字面量
        # 构造（含折叠常量容器）/delete/物化；每条独立成程序避免形态互相掩盖
        cases = [
            # 字面量与算术优先级（含括号、整除、取模、实数）
            'var a = 1 + 2 * 3; var b = (1 + 2) * 3; var c = 10 \\ 3; var d = 10 % 3;\n'
            'var e = 2.5 * 2; var f = 7 - (3 - 1); var g = 2 * 3 % 4;\n'
            'println(a); println(b); println(c); println(d); println(e); println(f); println(g);\n',
            # 位运算与移位
            'var x = 6 & 3 | 8 ^ 1; var y = 1 << 4 >> 1; var z = -16 >>> 2; var w = (6 | 1) & 3;\n'
            'println(x); println(y); println(z); println(w);\n',
            # 比较与逻辑（非短路双求值）
            'var p = 3 < 5 && 5 <= 5 || 1 > 2; var q = 1 == 1 != 0; var r = "1" === 1;\n'
            'var s = (1 || 0) && 0; var t = 1 !== 1;\n'
            'println(p); println(q); println(r); println(s); println(t);\n',
            # 一元族：负号、逻辑非、按位非（降糖 ^-1）、typeof、转换、#/$
            'var s = "abc"; println(-5); println(!0); println(~5); println(typeof s);\n'
            'println(int 2.9); println(real 3); println(string 42); println(#s); println($65);\n'
            'println(-(1 + 2)); println(!(1 && 0)); println(-s.length);\n',
            # 字符串转义与拼接
            'println("a\\"b\\\\c\\n" + "d"); println("x" + 1 + 2.5); println("" + (1 + 2));\n',
            # 数组/字典字面量（动态与折叠常量、嵌套）、下标读写、成员读写
            'var arr = [1, "s", 2.5]; arr[1] = "t"; println(arr[1]); println(arr.count);\n'
            'var d = %["k" => 1, "z" => "v"]; d.k = d.k + 4; println(d.k); println(d["z"]);\n'
            'var n = [[1, 2], %["a" => [3]]]; println(n[0][1]); println(n[1].a[0]);\n'
            'var fold = [1, 2, 3]; println(fold[2]); var fd = %["p" => 9]; println(fd.p);\n',
            # 方法调用、内置对象、链式成员
            'var arr = [3, 1, 2]; arr.sort(); println(arr.join(",")); println(Math.abs(-3));\n'
            'println("xyz".length); println([1, 2].join("-")); println(arr.count + 1);\n',
            # delete 语句形态与值形态（dst 被消费）
            'var dd = %["a" => 1, "b" => 2]; delete dd.a; println(dd.b);\n'
            'var ok = delete dd.b; println(ok); println(dd.count);\n',
            # 顶层多使用纯读复制（同一值分发到多处）
            'var k = 5; var m = k + k; var o = [k, k]; println(m); println(o[1]);\n',
        ]
        for index, source in enumerate(cases):
            with self.subTest(case=index):
                original = self.root / f"rt{index}.tjs"
                original.write_text(source, encoding="utf-8")
                expected = self.run_cial(original)
                self.assertEqual(expected.returncode, 0, expected.stderr)
                decompiled = self.run_cial("--decompile", original)
                self.assertEqual(decompiled.returncode, 0, decompiled.stderr)
                # 严格模式产物不含反汇编行
                self.assertNotIn("//   @", decompiled.stdout)
                recompiled = self.root / f"rt{index}_rt.tjs"
                recompiled.write_text(decompiled.stdout, encoding="utf-8")
                actual = self.run_cial(recompiled)
                self.assertEqual(actual.returncode, 0, actual.stderr + "\n--- decompiled ---\n" + decompiled.stdout)
                self.assertEqual(actual.stdout, expected.stdout, "--- decompiled ---\n" + decompiled.stdout)

    def test_decompile_reflects_prefix_unary_precedence(self):
        # 前缀一元紧于乘除、松于后缀（tjs.y unary_expr）：-a * 2 发射 ChgSign 再 Mul，
        # 反编译按 TAC 结构不加括号；显式 -(a * 2) 与 int o.b 保持原形
        self.script.write_text('var a = 7; var o = %["b" => 3];\n'
                               'println(-a * 2); println(-(a * 2)); println(int o.b + 1);\n',
                               encoding="utf-8")
        process = self.run_cial("--decompile", self.script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("println(-a * 2);", process.stdout)
        self.assertIn("println(-(a * 2));", process.stdout)
        self.assertIn("println(int o.b + 1);", process.stdout)

    def test_decompile_strict_rejects_structure_with_batch_reason(self):
        # 语句结构（M3）、声明装配（M4）在严格模式如实拒绝：退出码 2、不产文本、原因含批次号
        for source, batch in [("var a = 1;\nif (a) { println(a); }\n", "M3"),
                              ("function f() { return 1; }\n", "M4"),
                              ("var i = 0;\nwhile (i < 2) { i = i + 1; }\n", "M3")]:
            with self.subTest(batch=batch):
                self.script.write_text(source, encoding="utf-8")
                process = self.run_cial("--decompile", self.script)
                self.assertEqual(process.returncode, 2, process.stderr)
                self.assertEqual(process.stdout, "")
                self.assertIn(batch, process.stderr)


if __name__ == "__main__":
    unittest.main(argv=[__file__, *rest])
