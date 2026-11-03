"""cial-fmt CLI, literal preservation, idempotence and compiler round-trip tests."""
import argparse
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest


class FormatterTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not FORMATTER.is_file() or not CIAL.is_file():
            raise FileNotFoundError('Build cial-fmt and cial before running this test')

    def invoke(self, *arguments, source=None, cwd=None):
        return subprocess.run([str(FORMATTER), *map(str, arguments)], input=source,
                              cwd=cwd, capture_output=True, timeout=20)

    def formatted(self, source):
        result = self.invoke(source=source.encode() if isinstance(source, str) else source)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
        return result.stdout

    def golden(self, before, after):
        formatted = self.formatted(before)
        self.assertEqual(formatted, after.encode())
        self.assertEqual(self.formatted(formatted), formatted)

    def test_blocks_and_spacing(self):
        self.golden('function f(x,y){var a=x+y;if(a>0){return a;}else{return -a;}}',
                    'function f(x, y) {\n    var a = x + y;\n    if (a > 0) {\n        return a;\n    } else {\n        return -a;\n    }\n}\n')

    def test_for_headers_and_empty_blocks(self):
        self.golden('for(var i=0;i<3;++i){f(i);}for(;;){}',
                    'for (var i = 0; i < 3; ++i) {\n    f(i);\n}\nfor (;;) {}\n')

    def test_arrays_dictionary_and_unary_seams(self):
        self.golden('var a=[1,2,3];var d=%["x"=>a[0],"y"=>-1];a[1]=+ +a[0];',
                    'var a = [1, 2, 3];\nvar d = %["x" => a[0], "y" => -1];\na[1] = + +a[0];\n')

    def test_switch_case_indentation(self):
        self.golden('switch(x){case 1:a();break;default:b();}',
                    'switch (x) {\n    case 1:\n        a();\n        break;\n    default:\n        b();\n}\n')

    def test_unbraced_bodies_and_dangling_else(self):
        self.golden('if(a)if(b)f();else g();else h();after();',
                    'if (a)\n    if (b)\n        f();\n    else\n        g();\nelse\n    h();\nafter();\n')
        self.golden('for(;;)if(x)f();do f();while(x);',
                    'for (;;)\n    if (x)\n        f();\ndo\n    f();\nwhile (x);\n')
        self.golden('if(a){f();}else if(b)g();else h();',
                    'if (a) {\n    f();\n} else if (b)\n    g();\nelse\n    h();\n')

    def test_variadic_parameters_and_forwarding(self):
        self.golden('function collect(args*){return args;}var a=collect([1,2]*);',
                    'function collect(args*) {\n    return args;\n}\nvar a = collect([1, 2]*);\n')

    def test_multiline_containers_and_nested_functions(self):
        self.golden('var a=[\n1,2\n];call((function(){return 1;})());',
                    'var a = [\n    1,\n    2\n];\ncall((function () {\n    return 1;\n})());\n')

    def test_literal_and_comment_payloads(self):
        source = r'''// 字面量
var s="a  b\\n\\\" c";var r=/a  b[{};]/gi;var b=<% 00, ff 5 %>;
var t=@"x &name; ${1 + 2}";/* outer /* nested */ comment */
'''
        result = self.formatted(source)
        for literal in [r'"a  b\\n\\\" c"', '/a  b[{};]/gi', '<% 00, ff 5 %>',
                        '@"x &name; ${1 + 2}"', '/* outer /* nested */ comment */', '// 字面量']:
            self.assertIn(literal.encode(), result)
        self.assertEqual(self.formatted(result), result)

    def test_member_access_type_annotations_and_conditionals(self):
        self.golden('function f(x:int):int{return -x;}var n=1 .toString();',
                    'function f(x: int): int {\n    return -x;\n}\nvar n = 1 .toString();\n')
        self.golden('if(x)-f();else -g();var a=b?c:-d;',
                    'if (x)\n    -f();\nelse\n    -g();\nvar a = b ? c : -d;\n')
        self.golden('switch/*x*/(v){case 1:a.default();break;}',
                    'switch /*x*/ (v) {\n    case 1:\n        a.default();\n        break;\n}\n')

    def test_deep_and_arbitrary_input_terminate(self):
        import random
        rng = random.Random(42)
        self.assertEqual(self.invoke(source=b'{' * 513 + b'}' * 513).returncode, 2)
        for _ in range(80):
            source = ''.join(rng.choice('abc;{}[]()+-/*@"\\ \n') for _ in range(60)).encode()
            result = self.invoke(source=source)
            self.assertIn(result.returncode, (0, 2), (source, result.stderr))
            if result.returncode == 0:
                self.assertEqual(self.formatted(result.stdout), result.stdout)

    def test_comment_attachment(self):
        self.golden('var a=1;// trailing\n/* head */\nvar b=2;',
                    'var a = 1; // trailing\n/* head */\nvar b = 2;\n')

    def test_preprocessor_is_not_executed_and_regions_are_retained(self):
        region = '@if (0)\nvar invalid=;\n@if(1)\nx=2;\n@endif\n@endif'
        result = self.formatted('var a=1;\n@set(x=1/0)\n' + region + '\nvar b=2;')
        self.assertIn(region.encode(), result)
        self.assertIn(b'@set(x=1/0)', result)
        self.assertTrue(result.startswith(b'var a = 1;\n'))
        self.assertTrue(result.endswith(b'var b = 2;\n'))
        self.assertEqual(self.formatted(result), result)

    def test_conditional_regex_context_is_conservative(self):
        # 是否启用分支会影响 '/' 是正则还是除法；不对不明确的上下文猜测重排。
        for source in [b'@if(flag)\nx\n@endif\n/a b/;',
                       b'@if(flag) x @endif /* note */ @set(y=1) /a b/;',
                       b'@if(1) @if(flag) x @endif /a b/; @endif']:
            result = self.invoke(source=source)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b'')
        region = '@if(1)\nvar s="@endif";var r=/@endif/;\n@endif'
        result = self.formatted(region + '\nvar x=1;')
        self.assertIn(region.encode(), result)
        self.assertTrue(result.endswith(b'var x = 1;\n'))

    def test_bom_crlf_empty_and_blank_lines(self):
        self.assertEqual(self.formatted(b''), b'')
        self.assertEqual(self.formatted(b' \r\n \t'), b'')
        source = b'\xef\xbb\xbfvar x=1;\r\nvar y=2;\r\n'
        self.assertEqual(self.formatted(source), b'\xef\xbb\xbfvar x = 1;\r\nvar y = 2;\r\n')
        self.golden('var x=1;\n\n\n\nvar y=2;', 'var x = 1;\n\nvar y = 2;\n')

    def test_invalid_input_is_not_written(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            good = root / 'good.tjs'
            bad = root / 'bad.tjs'
            good.write_bytes(b'var x=1;')
            for source in [b'var x="unterminated', b'/* unclosed', b'var x=[1,2;', b'var x=\xff;',
                           b'var x=\0;', b'var\vx=1;', b'@if(1)\nvar x=1;', b'var r=/unclosed', b'var t=@"bad', b'var t=@"bad ${1', b'var t=@"bad &x']:
                with self.subTest(source=source):
                    bad.write_bytes(source)
                    result = self.invoke('--write', good, bad)
                    self.assertEqual(result.returncode, 2)
                    self.assertTrue(result.stderr)
                    self.assertEqual(good.read_bytes(), b'var x=1;')
                    self.assertEqual(bad.read_bytes(), source)
                    self.assertEqual(sorted(p.name for p in root.iterdir()), ['bad.tjs', 'good.tjs'])

    def test_write_check_paths_permissions_and_noop(self):
        with tempfile.TemporaryDirectory(prefix='cial fmt ') as directory:
            path = Path(directory) / '中文 文件.tjs'
            path.write_bytes(b'function f(){return 1;}')
            if os.name != 'nt':
                path.chmod(0o640)
            checked = self.invoke('--check', path)
            self.assertEqual(checked.returncode, 1)
            self.assertIn('中文 文件.tjs'.encode(), checked.stdout)
            self.assertEqual(path.read_bytes(), b'function f(){return 1;}')
            result = self.invoke('-i', '--indent-width=2', path)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(path.read_bytes(), b'function f() {\n  return 1;\n}\n')
            if os.name != 'nt':
                self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o640)
            timestamp = path.stat().st_mtime_ns
            self.assertEqual(self.invoke('--write', '--indent-width', '2', path, path).returncode, 0)
            self.assertEqual(path.stat().st_mtime_ns, timestamp)
            self.assertEqual(self.invoke('--check', '--indent-width', '2', path).returncode, 0)

    @unittest.skipIf(os.name == 'nt', 'Windows symlink creation needs an optional privilege')
    def test_write_rejects_symlinks(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'source.tjs'
            path.write_bytes(b'var x=1;')
            link = Path(directory) / 'alias.tjs'
            link.symlink_to(path)
            self.assertEqual(self.invoke('--write', link).returncode, 2)
            self.assertTrue(link.is_symlink())
            self.assertEqual(path.read_bytes(), b'var x=1;')

    def test_cli_errors_and_stdin_check(self):
        for arguments in [('--write',), ('--write', '--check'), ('--indent-width', '0'),
                          ('--indent-width=17',), ('--indent-width', 'bad'), ('--indent-width',),
                          ('--unknown',), ('-', '-'), ('missing-file.tjs',), ('one', 'two')]:
            self.assertEqual(self.invoke(*arguments, source=b'var x=1;').returncode, 2, arguments)
        self.assertEqual(self.invoke('--check', '-', source=b'var x=1;').returncode, 1)
        self.assertEqual(self.invoke('--check', source=b'var x = 1;\n').returncode, 0)
        self.assertIn(b'Usage:', self.invoke('--help').stdout)
        self.assertEqual(self.invoke('--version').stdout.strip().decode(), VERSION)
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / '-leading.tjs').write_bytes(b'var x=1;')
            self.assertEqual(self.invoke('--', '-leading.tjs', cwd=directory).returncode, 0)

    def test_runtime_round_trip(self):
        cases = [
            'var sum=0;for(var i=0;i<5;++i){sum+=i;}println(sum);',
            'function f(x){if(x>1){return x*2;}else{return -x;}}println(f(3));',
            'var a=[1,2,3];var d=%["x"=>a[1]];println(d.x);',
            'var x=3;var t=@"hello &x;";println(t);',
            'var r=/a+b/g;println(r.test("aab"));var x=12;x/=3;println(x);',
            'class A{var x=2;function f(){return x;}}var a=new A();println(a.f());',
            'switch(2){case 1:println(1);break;case 2:println(2);break;default:println(3);}',
            'try{throw 7;}catch(e){println(e);}',
            'var x=1;switch(1){case (function(){switch(x){case 1:return 1;}})():println(1);}',
        ]
        with tempfile.TemporaryDirectory() as directory:
            for index, source in enumerate(cases):
                with self.subTest(source=source):
                    original = Path(directory) / f'original-{index}.tjs'
                    formatted = Path(directory) / f'formatted-{index}.tjs'
                    original.write_text(source, encoding='utf-8')
                    formatted.write_bytes(self.formatted(source))
                    a = subprocess.run([str(CIAL), str(original)], capture_output=True, timeout=15)
                    b = subprocess.run([str(CIAL), str(formatted)], capture_output=True, timeout=15)
                    self.assertEqual(a.returncode, 0, a.stderr)
                    self.assertEqual(b.returncode, 0, b.stderr)
                    self.assertEqual((a.stdout, a.stderr), (b.stdout, b.stderr))

    def test_script_corpus_is_idempotent_and_still_compiles(self):
        root = Path(__file__).parent
        scripts = sorted((root / 'tjs').rglob('*.tjs'))
        self.assertGreater(len(scripts), 50)
        with tempfile.TemporaryDirectory() as directory:
            formatted = Path(directory) / 'formatted.tjs'
            for script in scripts:
                with self.subTest(script=script.relative_to(root)):
                    result = self.invoke(script)
                    self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
                    self.assertEqual(self.formatted(result.stdout), result.stdout)
                    formatted.write_bytes(result.stdout)
                    before = subprocess.run([str(CIAL), '--check-json', str(script)], capture_output=True, timeout=15)
                    after = subprocess.run([str(CIAL), '--check-json', str(formatted)], capture_output=True, timeout=15)
                    self.assertEqual(before.returncode, 0, before.stdout)
                    self.assertEqual(after.returncode, 0, after.stdout)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--formatter', type=Path, required=True)
    parser.add_argument('--cial', type=Path, required=True)
    parser.add_argument('--expected-version', required=True)
    args, remaining = parser.parse_known_args()
    FORMATTER, CIAL, VERSION = args.formatter.resolve(), args.cial.resolve(), args.expected_version
    unittest.main(argv=[__file__, *remaining])
