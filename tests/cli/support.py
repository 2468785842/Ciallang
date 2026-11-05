"""CLI 进程测试共用的参数、临时目录与语料路径。"""
import argparse
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

TJSBC_CORPUS = pathlib.Path(__file__).resolve().parents[1] / "test_files" / "tjsbc"


class CliTestCase(unittest.TestCase):
    cial = None
    expected_version = None

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
        return subprocess.run([self.cial, *map(str, arguments)], input=input, text=True,
                              encoding="utf-8", capture_output=True, timeout=15,
                              cwd=self.root, env=environment)


def run_tests():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cial", required=True)
    parser.add_argument("--expected-version")
    args, remaining = parser.parse_known_args()
    CliTestCase.cial = str(pathlib.Path(args.cial).resolve())
    CliTestCase.expected_version = args.expected_version
    unittest.main(argv=[sys.argv[0], *remaining])
