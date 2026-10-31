"""Run self-checking TJS files through Ciallang or a script-path-aware TJSCompiler."""

import argparse
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys


TEST_ROOT = Path(__file__).resolve().parent / "tjs"
PASS_MARKER = re.compile(rb"__TJS_TEST_PASS__:([0-9]+)")


def run_case(engine, script, timeout):
    environment = os.environ.copy()
    environment.pop("CIAL_DUMP", None)
    environment.pop("CIAL_TJSBC_DUMP", None)
    result = {"script": str(script), "engine": str(engine), "assertions": 0}
    try:
        process = subprocess.run([str(engine), str(script)], capture_output=True,
                                 timeout=timeout, env=environment)
    except (OSError, subprocess.TimeoutExpired) as error:
        result.update(status="ERROR", error=str(error))
        return result
    markers = PASS_MARKER.findall(process.stdout)
    passed = process.returncode == 0 and len(markers) == 1 and int(markers[0]) > 0
    result.update(status="PASS" if passed else "FAIL", returncode=process.returncode,
                  stdout=process.stdout.decode("utf-8", errors="replace"),
                  stderr=process.stderr.decode("utf-8", errors="replace"))
    if passed:
        result["assertions"] = int(markers[0])
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True, type=Path,
                        help="cial or TJSCompiler executable accepting a script path")
    parser.add_argument("--timeout", type=float, default=10)
    parser.add_argument("--report", type=Path, help="write all results, including failure output, as JSON")
    parser.add_argument("paths", nargs="*", type=Path, help="test files or directories; defaults to tests/tjs")
    args = parser.parse_args(argv)
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout must be a positive finite number")
    engine = args.engine.resolve()
    if not engine.is_file():
        parser.error(f"engine does not exist: {engine}")
    scripts = set()
    for path in args.paths or [TEST_ROOT]:
        if not path.exists():
            parser.error(f"test path does not exist: {path}")
        candidates = path.rglob("*.tjs") if path.is_dir() else [path]
        scripts.update(p.resolve() for p in candidates if p.name != "assert.tjs")
    if not scripts:
        parser.error("no test files selected")
    results = [run_case(engine, script, args.timeout) for script in sorted(scripts)]
    for result in results:
        if result["status"] != "PASS":
            print(f"{result['status']}: {result['script']}")
            print(result.get("error", f"exit code: {result.get('returncode')}"))
            for stream in ("stdout", "stderr"):
                if result.get(stream):
                    print(f"{stream}:\n{result[stream]}")
    passed = sum(result["status"] == "PASS" for result in results)
    assertions = sum(result["assertions"] for result in results)
    print(f"{engine.name}: {passed}/{len(results)} files passed, {assertions} assertions")
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(results, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
