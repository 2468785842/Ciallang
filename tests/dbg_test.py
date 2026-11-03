"""Exercise the cial-dbg terminal debugger: breakpoints, stepping, frames, abort."""
import argparse
import os
import pathlib
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument("--dbg", required=True)
args, rest = parser.parse_known_args()
DBG = str(pathlib.Path(args.dbg).resolve())


class DbgTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)
        self.script = self.root / "script.tjs"

    def run_dbg(self, script_text, commands):
        self.script.write_text(script_text, encoding="utf-8")
        environment = dict(os.environ)
        environment.pop("CIAL_DUMP", None)
        environment.pop("CIAL_TJSBC_DUMP", None)
        return subprocess.run([DBG, str(self.script)], input=commands, text=True,
                              encoding="utf-8", capture_output=True, timeout=15,
                              cwd=self.root, env=environment)

    def test_breakpoint_locals_and_finish(self):
        result = self.run_dbg(
            "function add(a, b) {\n"
            "    var t = a + b;\n"
            "    return t;\n"
            "}\n"
            "var x = 10;\n"
            "var y = add(x, 5);\n"
            "println(y);\n",
            "break 6\nrun\nnext\nprint y\ncontinue\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Breakpoint 1 at", result.stdout)
        self.assertIn("Stopped at", result.stdout)
        self.assertIn("script.tjs:6", result.stdout)
        self.assertIn("y = 15", result.stdout)
        self.assertIn("program finished", result.stdout)

    def test_step_into_function_and_frames(self):
        result = self.run_dbg(
            "function add(a, b) {\n"
            "    var t = a + b;\n"
            "    return t;\n"
            "}\n"
            "var y = add(1, 2);\n"
            "println(y);\n",
            "break 5\nrun\nstep\nlocals\nprint a\nbt\nframe 1\ncontinue\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("(add)", result.stdout)
        self.assertIn("a = 1", result.stdout)
        self.assertIn("b = 2", result.stdout)
        self.assertIn("a = 1", result.stdout)
        self.assertIn("#0", result.stdout)
        self.assertIn("#1", result.stdout)
        self.assertIn("#1 <script>", result.stdout)
        self.assertIn("program finished", result.stdout)

    def test_debugger_statement_stops(self):
        result = self.run_dbg(
            "var a = 1;\n"
            "debugger;\n"
            "println(a);\n",
            "continue\nprint a\ncontinue\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("[debugger statement]", result.stdout)
        self.assertIn("script.tjs:2", result.stdout)
        self.assertIn("a = 1", result.stdout)
        self.assertIn("program finished", result.stdout)

    def test_quit_aborts_execution(self):
        result = self.run_dbg(
            "var a = 1;\n"
            "println(a);\n"
            "println(a);\n",
            "break 2\nrun\nquit\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("program finished", result.stdout)

    def test_breakpoint_without_code_is_rejected(self):
        result = self.run_dbg(
            "var a = 1;\n"
            "var b = 2;\n",
            "break 99\nrun\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("no code at or after line 99", result.stdout)
        self.assertIn("program finished", result.stdout)

    def test_delete_breakpoint_and_run_to_completion(self):
        result = self.run_dbg(
            "var a = 1;\n"
            "var b = 2;\n"
            "println(a + b);\n",
            "break 2\nrun\ndelete 1\ncontinue\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("deleted breakpoint 1", result.stdout)
        self.assertIn("program finished", result.stdout)
        self.assertIn("3", result.stdout)


if __name__ == "__main__":
    unittest.main(argv=[__file__, *rest])
