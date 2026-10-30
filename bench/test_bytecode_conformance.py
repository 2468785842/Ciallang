"""Tool contract tests; optional --cial adds a real subprocess/fixture comparison."""
import argparse
import base64
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import bytecode_conformance as tool

parser = argparse.ArgumentParser()
parser.add_argument("--cial", type=Path)
args, rest = parser.parse_known_args()
CIAL = args.cial.resolve() if args.cial else None


def result(stdout=b"", returncode=0, timed_out=False):
    return {"stdout": stdout, "stderr": b"", "returncode": returncode, "timed_out": timed_out}


class ComparisonContractTest(unittest.TestCase):
    def test_exact_stdout_and_failures(self):
        self.assertEqual(tool.classify(result(b"x\n"), result(b"x\n")), "MATCH")
        for output in (b"x\r\n", b'"x"\n', b"x\n\n", b"x\x00\n"):
            self.assertEqual(tool.classify(result(b"x\n"), result(output)), "OUTPUT_DIFF")
        self.assertEqual(tool.classify(result(returncode=1), result(returncode=1)), "EXECUTION_FAILED")
        self.assertEqual(tool.classify(result(), result(timed_out=True)), "TIMEOUT")

    def test_difference_offsets_and_binary_report(self):
        self.assertIsNone(tool.first_difference(b"abc", b"abc"))
        self.assertEqual(tool.first_difference(b"abc", b"abcd"), 3)
        self.assertEqual(tool.first_difference(b"abc", b"axc"), 1)
        encoded = tool.encoded_run(result(b"\xff\x00\r\n"))
        self.assertEqual(base64.b64decode(encoded["stdout"]["base64"]), b"\xff\x00\r\n")

    def test_runner_sanitizes_dump_flags_and_reports_timeout(self):
        with mock.patch.dict(tool.os.environ, {"CIAL_DUMP": "1", "CIAL_TJSBC_DUMP": "all"}), \
                mock.patch.object(tool.subprocess, "run") as process:
            process.side_effect = subprocess.TimeoutExpired(["cial"], 0.1, output=b"partial")
            output = tool.run(["cial", "script.tjs"], 0.1, Path.cwd())
            self.assertTrue(output["timed_out"])
            self.assertEqual(output["stdout"], b"partial")
            self.assertNotIn("CIAL_DUMP", process.call_args.kwargs["env"])
            self.assertNotIn("CIAL_TJSBC_DUMP", process.call_args.kwargs["env"])

    def test_fixed_script_reference_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(tool, "run", return_value=result(b"fixed")):
            with self.assertRaisesRegex(RuntimeError, "script-path"):
                tool.probe_reference(Path("reference"), Path("cial"), 1, Path(temporary))

    def test_export_with_success_exit_but_bad_header_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(tool, "run", return_value=result()):
            output = Path(temporary) / "out.tjsbc"
            output.write_bytes(b"not bytecode")
            with self.assertRaisesRegex(RuntimeError, "TJS2 100"):
                tool.export(Path("reference"), Path("in.tjs"), output, 1, Path(temporary))

    def test_probe_checks_both_inputs_and_executes_export(self):
        seen = []
        def run(command, timeout, cwd):
            if "--dump" in command:
                Path(command[-1]).write_bytes(tool.MAGIC)
                return result()
            source = Path(command[-1]).with_suffix(".tjs")
            marker = source.read_text().split('"')[1].encode()
            seen.append((command[0], marker))
            return result(marker)
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(tool, "run", side_effect=run):
            reference = Path(temporary) / "reference"
            reference.write_bytes(b"reference binary")
            report = tool.probe_reference(reference, Path("cial"), 1, Path(temporary))
            self.assertTrue(report["export_execution_probe"])
            self.assertEqual(len(seen), 4)
            self.assertEqual(len({marker for _, marker in seen}), 2)

    def test_report_does_not_accept_two_matching_failures(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            executable = directory / "cial"
            executable.write_bytes(b"binary")
            (directory / "case.tjs").write_text("throw 1;")
            (directory / "case.tjsbc").write_bytes(tool.MAGIC)
            report_path = directory / "report.json"
            with mock.patch.object(tool, "run", return_value=result(returncode=1)), \
                    contextlib.redirect_stdout(io.StringIO()):
                code = tool.main(["--cial", str(executable), "--fixture-dir", str(directory),
                                  "--report", str(report_path)])
            report = json.loads(report_path.read_text())
            self.assertEqual(code, 1)
            self.assertFalse(report["ok"])
            self.assertEqual(report["summary"], {"EXECUTION_FAILED": 1})

    def test_missing_sources_are_counted_but_cannot_make_a_successful_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            executable = directory / "cial"
            executable.write_bytes(b"binary")
            (directory / "missing.tjsbc").write_bytes(tool.MAGIC)
            output = io.StringIO()
            with contextlib.redirect_stdout(output), mock.patch.object(tool, "run") as runner:
                code = tool.main(["--cial", str(executable), "--fixture-dir", str(directory),
                                  "--source-dir", str(directory), "--case", "missing"])
            report = json.loads(output.getvalue())
            self.assertEqual(code, 1)
            self.assertEqual(report["summary"], {"SKIPPED_NO_SOURCE": 1})
            runner.assert_not_called()

    def test_execution_mode_never_claims_source_equivalence(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            executable = directory / "cial"
            executable.write_bytes(b"binary")
            (directory / "missing.tjsbc").write_bytes(tool.MAGIC)
            output = io.StringIO()
            with contextlib.redirect_stdout(output), mock.patch.object(tool, "run", return_value=result(b"done")):
                code = tool.main(["--cial", str(executable), "--fixture-dir", str(directory), "--mode", "execute"])
            report = json.loads(output.getvalue())
            self.assertEqual(code, 0)
            self.assertEqual(report["coverage_mode"], "bytecode_execution")
            self.assertEqual(report["summary"], {"EXECUTED": 1})
            self.assertNotIn("source_run", report["cases"][0])

    def test_execution_mode_rejects_failure_and_timeout(self):
        with tempfile.TemporaryDirectory() as temporary:
            fixture = Path(temporary) / "case.tjsbc"
            fixture.write_bytes(tool.MAGIC)
            for process, expected in [(result(returncode=1), "EXECUTION_FAILED"),
                                      (result(timed_out=True), "TIMEOUT")]:
                with self.subTest(expected=expected), mock.patch.object(tool, "run", return_value=process):
                    self.assertEqual(tool.execute_case(Path("cial"), fixture, 1, Path(temporary))["status"], expected)


@unittest.skipUnless(CIAL, "provide --cial to test the actual CLI and fixtures")
class CliIntegrationTest(unittest.TestCase):
    def test_original_fixtures_without_source_have_execution_coverage(self):
        command = [sys.executable, str(Path(tool.__file__).resolve()), "--cial", str(CIAL), "--mode", "execute"]
        for name in ("s1", "s2", "s3", "s4", "s5"):
            command.extend(("--case", name))
        with tempfile.TemporaryDirectory() as temporary:
            process = subprocess.run(command, cwd=temporary, capture_output=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stderr)
        report = json.loads(process.stdout)
        self.assertEqual(report["summary"], {"EXECUTED": 5})
        self.assertEqual(report["coverage_mode"], "bytecode_execution")

    def test_real_fixture_from_another_working_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            process = subprocess.run([sys.executable, str(Path(tool.__file__).resolve()), "--cial", str(CIAL),
                                      "--case", "s7_ext", "--case", "test_class"], cwd=temporary,
                                     capture_output=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stderr)
        report = json.loads(process.stdout)
        self.assertEqual(report["summary"], {"MATCH": 2})
        self.assertTrue(all(case["first_different_stdout_byte"] is None for case in report["cases"]))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *rest])
