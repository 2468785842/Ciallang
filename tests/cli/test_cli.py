"""CLI、诊断和 REPL 的进程回归。"""
import json
import subprocess

from support import CliTestCase, TJSBC_CORPUS, run_tests


class CliTest(CliTestCase):
    def test_help_version_and_arguments(self):
        self.assertEqual(self.run_cial("--help").returncode, 0)
        self.assertIn("--repl", self.run_cial("--help").stdout)
        version = self.run_cial("--version")
        self.assertEqual(version.returncode, 0)
        if self.expected_version:
            self.assertEqual(version.stdout.strip(), self.expected_version)
        for flags in [("--unknown",), ("--tjsbc",), (self.script, "extra"),
                      ("--jit", "--trace-jit", self.script),
                      ("--check-json", "--dump-cfg", self.script),
                      ("--repl", self.script), ("--check-json", "--time", self.script),
                      ("--trace-execution", self.script)]:
            with self.subTest(flags=flags):
                process = self.run_cial(*flags)
                self.assertEqual(process.returncode, 2, process.stderr)
                self.assertEqual(process.stdout, "")

    def test_capabilities_json(self):
        # 能力查询（doc/roadmap.md §3.1）：单行 JSON，消费方按 schemaVersion 兼容
        process = self.run_cial("--capabilities")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, "")
        payload = json.loads(process.stdout)
        self.assertEqual(payload["schemaVersion"], 1)
        self.assertEqual(payload["sdkApiVersion"], 1)
        self.assertTrue(payload["version"])
        if self.expected_version:
            self.assertEqual(payload["version"], self.expected_version)
        if self.expected_jit is not None:
            # 报告值须与构建事实一致（CIAL_ENABLE_JIT），而非硬编码关闭
            self.assertEqual(payload["capabilities"]["jit"], self.expected_jit)
        # 互斥与不接受输入/执行选项
        for flags in [("--capabilities", "--tjsbc"), ("--capabilities", self.script),
                      ("--capabilities", "--time", self.script), ("--capabilities", "--trace-execution")]:
            with self.subTest(flags=flags):
                process = self.run_cial(*flags)
                self.assertEqual(process.returncode, 2, process.stderr)
                self.assertEqual(process.stdout, "")

    def test_api_json(self):
        # 注册期 API 元数据导出（doc/api-metadata.md）：schemaVersion 1，仅描述注册
        # 事实；签名与 src/stdlib 注册代码对照，推导不出的类型输出 null
        process = self.run_cial("--api-json")
        self.assertEqual(process.returncode, 0, process.stderr)
        payload = json.loads(process.stdout)
        self.assertEqual(payload["schemaVersion"], 1)
        if self.expected_version:
            self.assertEqual(payload["version"], self.expected_version)
        classes = {entry["name"]: entry["members"] for entry in payload["classes"]}
        # 内建容器与内建类型均有注册名与成员（对照 ArrayLib/StringLib 注册代码）
        self.assertIn("Array", classes)
        self.assertIn("String", classes)
        self.assertIn("Math", classes)
        self.assertIn("Scripts", classes)
        array = {m["name"]: m for m in classes["Array"]}
        self.assertTrue(array["push"]["variadic"])  # (VM*, Value, size_t, Value*) 变长形态
        self.assertEqual(array["count"]["kind"], "property")
        string = {m["name"]: m for m in classes["String"]}
        self.assertEqual(string["charAt"]["params"], ["Integer"])
        self.assertEqual(string["charAt"]["ret"], "String")
        self.assertEqual(string['charAt']['parameters'][0]['name'], 'index')
        self.assertEqual(array['join']['parameters'][0]['defaultText'], '","')
        self.assertEqual([param['name'] for param in array['split']['parameters']],
                         ['pattern', 'text', 'reserved', 'purgeempty'])
        self.assertEqual(array['sort']['ret'], 'void')
        self.assertTrue(array['count']['hasSetter'])
        math = {m["name"]: m for m in classes["Math"]}
        self.assertEqual(math["abs"]["kind"], "method")
        self.assertEqual(math["abs"]["variadic"], True)
        scripts = {m["name"]: m for m in classes["Scripts"]}
        self.assertEqual(scripts["execStorage"]["variadic"], True)
        # 全局函数（Session 注册链显式登记）
        globals_map = {g["name"]: g for g in payload["globals"]}
        self.assertEqual(globals_map["print"]["ret"], "void")
        self.assertEqual(globals_map["print"]["params"], ["any"])
        # 互斥与不接受输入/执行选项
        for flags in [("--api-json", "--check-json", self.script), ("--api-json", self.script),
                      ("--api-json", "--jit", self.script)]:
            with self.subTest(flags=flags):
                process = self.run_cial(*flags)
                self.assertEqual(process.returncode, 2, process.stderr)
                self.assertEqual(process.stdout, "")

    def test_api_decl(self):
        # 声明文件生成（doc/declarations.md §3）：与 --api-json 同一注册链，
        # 输出仅由模型决定、跨调用逐字节确定
        first = self.run_cial("--api-decl")
        self.assertEqual(first.returncode, 0, first.stderr)
        second = self.run_cial("--api-decl")
        self.assertEqual(first.stdout, second.stdout)
        self.assertTrue(first.stdout.endswith("\n"))
        self.assertIn("class String {", first.stdout)
        self.assertIn("function charAt(index: Integer): String;", first.stdout)
        self.assertIn("function push(values: any*): Integer;", first.stdout)
        self.assertIn('function join(delim: String = ","): String;', first.stdout)
        self.assertIn("property length: Integer;", first.stdout)
        self.assertIn("/** 输出值的字符串化结果（不换行） */", first.stdout)
        # 类集合与 --api-json 一致
        api = json.loads(self.run_cial("--api-json").stdout)
        for entry in api["classes"]:
            self.assertIn("class %s {" % entry["name"], first.stdout)
        # 互斥与不接受输入/执行选项
        for flags in [("--api-decl", "--api-json"), ("--api-decl", self.script),
                      ("--api-decl", "--time", self.script), ("--api-decl", "--dump-tac", self.script)]:
            with self.subTest(flags=flags):
                process = self.run_cial(*flags)
                self.assertEqual(process.returncode, 2, process.stderr)
                self.assertEqual(process.stdout, "")

    def test_native_export_declaration_symbol_consistency(self):
        api = json.loads(self.run_cial('--api-json').stdout)
        text = self.run_cial('--api-decl').stdout
        process = self.run_cial('--decl-json', '--source-name', 'stdlib.d.tjs', '-', input=text)
        self.assertEqual(process.returncode, 0, process.stderr)
        symbols = json.loads(process.stdout)['symbols']
        classes = {node['name']: node for node in symbols if node['kind'] == 'class'}
        for native in api['classes']:
            actual = {member['name']: member for member in classes[native['name']]['members']}
            self.assertEqual(set(actual), {member['name'] for member in native['members']})
            for metadata in native['members']:
                symbol = actual[metadata['name']]
                self.assertEqual(symbol['doc'], metadata.get('doc', ''))
                self.assertEqual(symbol['ret'], metadata['ret'] or '')
                if 'parameters' in metadata:
                    self.assertEqual(symbol['params'], metadata['parameters'])

    def test_core_declaration_symbols_json(self):
        text = '/** 中文接口 😀\x01 */\nclass Window {\n/** 显示标题 */\nfunction show(title: String = "标题"): void;\nproperty caption { getter: String; setter(value: String); }\n}\n'
        # The protocol counts the bytes actually transmitted, so avoid text-mode
        # newline translation by the Python client as well as by the native CLI.
        result = subprocess.run([self.cial, '--decl-json', '--source-name', 'file:///host.d.tjs', '-'],
                                input=text.encode('utf-8'), capture_output=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report['schemaVersion'], 1)
        self.assertTrue(report['check']['ok'])
        window = report['symbols'][0]
        self.assertEqual(window['doc'], '中文接口 😀\x01')
        show = window['members'][0]
        self.assertEqual(show['params'][0]['name'], 'title')
        self.assertEqual(show['params'][0]['defaultText'], '"标题"')
        self.assertTrue(show['params'][0]['optional'])
        self.assertEqual(text.encode('utf-8')[show['begin']:show['end']].decode('utf-8'), 'show')
        self.assertTrue(window['members'][1]['hasSetter'])
        raw = text.replace('\n', '\r\n').encode('utf-8')
        crlf = subprocess.run([self.cial, '--decl-json', '--source-name', 'host.d.tjs', '-'],
                              input=raw, capture_output=True, timeout=15)
        self.assertEqual(crlf.returncode, 0, crlf.stderr)
        raw_symbol = json.loads(crlf.stdout)['symbols'][0]['members'][0]
        self.assertEqual(raw[raw_symbol['begin']:raw_symbol['end']].decode('utf-8'), 'show')
        bad = self.run_cial('--decl-json', '--source-name', 'bad.d.tjs', '-', input='println("must not run");')
        self.assertEqual(bad.returncode, 1)
        self.assertFalse(json.loads(bad.stdout)['check']['ok'])
        self.assertNotIn('must not run\n', bad.stdout)
        flags = self.run_cial('--decl-json', '--time', '-')
        self.assertEqual(flags.returncode, 2)

    def test_declaration_check_json(self):
        # 声明检查（doc/declarations.md §4）：--check-json 对 .d.tjs 按声明模式分析
        generated = self.run_cial("--api-decl").stdout
        generated_file = self.root / "generated.d.tjs"
        generated_file.write_text(generated, encoding="utf-8")
        process = self.run_cial("--check-json", generated_file)
        self.assertEqual(process.returncode, 0, process.stdout)
        report = json.loads(process.stdout)
        self.assertTrue(report["ok"])
        self.assertEqual(report["diagnostics"], [])

        good = self.root / "good.d.tjs"
        good.write_text(
            "/** 文档 */\n"
            "class Thing extends Base {\n"
            "    function make(name: String = \"x\"): Thing;\n"
            "    property value: Integer;\n"
            "    var table: Dictionary;\n"
            "}\n"
            "namespace Nested { function helper(*): void; }\n"
            "function top(a, b: any = null): void;\n", encoding="utf-8")
        process = self.run_cial("--check-json", good)
        self.assertEqual(process.returncode, 0, process.stdout)
        self.assertTrue(json.loads(process.stdout)["ok"])

        bad = self.root / "bad.d.tjs"
        # 诊断同步只在声明关键字处停驻，非声明语句放最前避免被跳过
        bad.write_text("if (x) {}\nvar x = 1;\nfunction f(a = (1));\n", encoding="utf-8")
        process = self.run_cial("--check-json", bad)
        self.assertEqual(process.returncode, 1)
        report = json.loads(process.stdout)
        self.assertFalse(report["ok"])
        codes = [entry["code"] for entry in report["diagnostics"]]
        self.assertIn("decl.initializer", codes)
        self.assertIn("decl.invalid-default", codes)
        self.assertIn("decl.not-a-declaration", codes)

        # stdin + --source-name 识别声明身份（LSP 文档路径）
        process = self.run_cial("--check-json", "--source-name", "virtual.d.tjs", "-", input="function f();")
        self.assertEqual(process.returncode, 0, process.stdout)
        self.assertTrue(json.loads(process.stdout)["ok"])

    def test_declaration_execution_refused(self):
        # .d.tjs 不可当脚本执行（doc/declarations.md §4），编译型模式同样拒绝
        declaration = self.root / "lib.d.tjs"
        declaration.write_text("function f(): void;\n", encoding="utf-8")
        for flags in [(), ("--dump-cfg",), ("--dump-ssa",), ("--decompile",)]:
            with self.subTest(flags=flags):
                process = self.run_cial(*flags, declaration)
                self.assertEqual(process.returncode, 1)
                self.assertIn("decl.not-executable", process.stderr)
                self.assertEqual(process.stdout, "")
        # --tjsbc 走字节码探测，不因扩展名误报声明拒绝
        process = self.run_cial("--tjsbc", declaration)
        self.assertNotIn("decl.not-executable", process.stderr)

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
do {
        x = x + 1; } while(x < 44)
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
        # 指示符按显示宽度对齐：栏 7 列 + 前缀 16 显示列（中文各 2 列） + "var b = " 8 列
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
        corpus = TJSBC_CORPUS
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
        if self.expected_jit is False:
            self.assertEqual(process.returncode, 1)
            self.assertIn("JIT backend not enabled", process.stderr)
        elif process.returncode == 0:  # expected_jit 为 True 或未知时
            self.assertEqual(process.stdout, "42\n")
            self.assertIn("[jit]", process.stderr)
        else:
            self.assertEqual(process.returncode, 1)
            self.assertIn("JIT backend not enabled", process.stderr)


if __name__ == "__main__":
    run_tests()
