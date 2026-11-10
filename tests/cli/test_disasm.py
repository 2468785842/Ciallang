"""cial-disasm 进程回归：字节码/源码双路径、行号换算、行表 JSON 与退出码。"""
import argparse
import json
import os
import pathlib
import subprocess
import tempfile
import unittest

TJSBC_CORPUS = pathlib.Path(__file__).resolve().parents[1] / "test_files" / "tjsbc"

parser = argparse.ArgumentParser()
parser.add_argument("--disasm", required=True)
args, rest = parser.parse_known_args()
DISASM = str(pathlib.Path(args.disasm).resolve())


class DisasmTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)

    def run_disasm(self, *arguments):
        environment = dict(os.environ)
        environment.pop("CIAL_DUMP", None)
        environment.pop("CIAL_TJSBC_DUMP", None)
        return subprocess.run([DISASM, *map(str, arguments)], text=True, encoding="utf-8",
                              capture_output=True, timeout=15, cwd=self.root, env=environment)

    def test_bytecode_corpus_contexts_and_no_recovery(self):
        corpus = TJSBC_CORPUS / "debug"
        sample = corpus / "s7_ext.tjsbc"
        process = self.run_disasm(sample)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, "")
        # 纯反汇编：上下文头 + 逐指令行；不尝试恢复，故无恢复注释与 unsupported 文案
        self.assertIn("// ==== context", process.stdout)
        self.assertRegex(process.stdout, r"//   @0\s+\w+")
        self.assertNotIn("//   >>", process.stdout)
        self.assertNotIn("unsupported", process.stdout.lower())
        self.assertIn("not recompilable", process.stdout)
        # 已装配进壳的 context 也独立发射（指令全量覆盖，不依赖恢复）
        self.assertGreaterEqual(process.stdout.count("// ==== context"), 2)

    def test_source_path_compiles_and_disassembles(self):
        script = self.root / "script.tjs"
        script.write_text("function add(a, b) {\n    return a + b;\n}\nprintln(add(1, 2));\n",
                          encoding="utf-8")
        process = self.run_disasm(script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("// ==== top-level ====", process.stdout)
        self.assertRegex(process.stdout, r"@\d+\s+call")
        # 嵌套函数以常量图单元独立发射
        self.assertIn("// ==== function", process.stdout)
        self.assertNotIn("//   >>", process.stdout)

    def test_source_flag_maps_offsets_to_lines(self):
        corpus = TJSBC_CORPUS / "debug"
        process = self.run_disasm("--source", corpus / "lines.tjs", corpus / "lines.tjsbc")
        self.assertEqual(process.returncode, 0, process.stderr)
        # 行号换算只应增注释列（; 之后），指令列（pc/操作码/操作数）保持不变
        plain = self.run_disasm(corpus / "lines.tjsbc")
        self.assertEqual(plain.returncode, 0, plain.stderr)
        mapped_lines = process.stdout.splitlines()
        unmapped_lines = plain.stdout.splitlines()
        self.assertEqual(len(mapped_lines), len(unmapped_lines))
        for mapped, unmapped in zip(mapped_lines, unmapped_lines):
            self.assertEqual(mapped.split(";")[0], unmapped.split(";")[0])
        self.assertRegex(process.stdout, r"w22 src@99 L7")

    def test_map_json_structure_and_output_file(self):
        script = self.root / "script.tjs"
        script.write_text("var v = 41 + 1;\nprintln(v);\n", encoding="utf-8")
        output = self.root / "disasm.txt"
        map_path = self.root / "map.json"
        process = self.run_disasm("--output", output, "--map", map_path, script)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout, "")
        text = output.read_text(encoding="utf-8")
        self.assertIn("// ==== top-level ====", text)
        self.assertIn("load_imm", text)
        payload = json.loads(map_path.read_text(encoding="utf-8"))
        self.assertEqual(payload["status"], "success")
        self.assertTrue(payload["units"])
        for unit in payload["units"]:
            for row in unit["lines"]:
                self.assertIn("textLine", row)
                self.assertIn("tac", row)
        # 同一路径写 --output 与 --map 属用法错误
        process = self.run_disasm("--output", output, "--map", output, script)
        self.assertEqual(process.returncode, 2)
        self.assertIn("cial-disasm:", process.stderr)

    def test_missing_and_garbage_inputs(self):
        process = self.run_disasm(self.root / "missing.tjsbc")
        self.assertEqual(process.returncode, 2)
        self.assertIn("cial-disasm:", process.stderr)
        garbage = self.root / "garbage.bin"
        garbage.write_bytes(b"\x01\x02\x03\x04 not a script or bytecode")
        # 无魔数按源码编译，解析失败归入编译失败
        process = self.run_disasm(garbage)
        self.assertEqual(process.returncode, 1)
        process = self.run_disasm()
        self.assertEqual(process.returncode, 2)


if __name__ == "__main__":
    unittest.main(argv=["cial-disasm-test", *rest])
