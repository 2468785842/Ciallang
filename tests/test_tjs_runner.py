import argparse
import contextlib
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import run_tjs_tests as runner


ENGINE = None


class RunnerTest(unittest.TestCase):
    def test_success_requires_a_single_positive_completion_marker_and_zero_exit(self):
        for output, code, expected in [
            (b'"__TJS_TEST_PASS__:3"\n', 0, "PASS"),
            (b"12:34:56 __TJS_TEST_PASS__:3\n", 0, "PASS"),
            (b"", 0, "FAIL"),
            (b"__TJS_TEST_PASS__:0", 0, "FAIL"),
            (b"__TJS_TEST_PASS__:3", 1, "FAIL"),
            (b"__TJS_TEST_PASS__:3\n__TJS_TEST_PASS__:3", 0, "FAIL"),
        ]:
            with self.subTest(output=output, code=code), mock.patch.object(
                runner.subprocess, "run", return_value=subprocess.CompletedProcess([], code, output, b"")
            ):
                self.assertEqual(runner.run_case(Path("engine"), Path("test.tjs"), 1)["status"], expected)

    def test_timeout_and_launch_error_are_failures(self):
        for error in (subprocess.TimeoutExpired([], 0.01), OSError("cannot execute")):
            with self.subTest(error=error), mock.patch.object(runner.subprocess, "run", side_effect=error):
                self.assertEqual(runner.run_case(Path("engine"), Path("test.tjs"), 1)["status"], "ERROR")

    def test_missing_case_is_an_error_instead_of_an_empty_passing_suite(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
            runner.main(["--engine", __file__, "missing-test-file.tjs"])
        self.assertEqual(error.exception.code, 2)


class InterpreterTest(unittest.TestCase):
    def setUp(self):
        if ENGINE is None:
            self.skipTest("provide --engine for actual interpreter checks")

    def run_source(self, source):
        with tempfile.TemporaryDirectory(prefix="cial-tjs-runner-") as temporary:
            script = Path(temporary) / "case.tjs"
            helper = (runner.TEST_ROOT / "assert.tjs").as_posix()
            script.write_text(f'Scripts.execStorage("{helper}");\n{source}\n', encoding="utf-8")
            return runner.run_case(ENGINE, script, 5)

    def test_assertion_failure_is_detected(self):
        result = self.run_source('assertTrue(false, "negative control"); finishTests();')
        self.assertEqual(result["status"], "FAIL")
        self.assertNotEqual(result["returncode"], 0)

    def test_premature_exit_and_zero_assertions_do_not_pass(self):
        for source in ("var x = 1;", "finishTests();"):
            with self.subTest(source=source):
                self.assertEqual(self.run_source(source)["status"], "FAIL")

    def test_compilation_failure_does_not_pass(self):
        self.assertEqual(self.run_source("var ;")["status"], "FAIL")

    def test_script_and_shared_assertions_work_from_a_different_directory(self):
        result = self.run_source('assertTrue(40 + 2 == 42); finishTests();')
        self.assertEqual(result["status"], "PASS", result)
        self.assertEqual(result["assertions"], 1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--engine", type=Path)
    options, remaining = parser.parse_known_args()
    if options.engine:
        ENGINE = options.engine.resolve()
    unittest.main(argv=[__file__, *remaining])
