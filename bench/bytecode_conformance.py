"""Compare Ciallang source and TJS2 bytecode execution without normalizing stdout."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[1]
MAGIC = b"TJS2100\0"


def run(command, timeout, cwd):
    environment = dict(os.environ)
    for name in ("CIAL_DUMP", "CIAL_TJSBC_DUMP", "CIAL_OPTIMIZER_STATS"):
        environment.pop(name, None)
    try:
        process = subprocess.run(list(map(str, command)), capture_output=True, timeout=timeout,
                                 cwd=cwd, env=environment)
        return {"returncode": process.returncode, "stdout": process.stdout, "stderr": process.stderr,
                "timed_out": False}
    except subprocess.TimeoutExpired as error:
        return {"returncode": None, "stdout": error.stdout or b"", "stderr": error.stderr or b"",
                "timed_out": True}


def encoded_run(result):
    return {**{key: value for key, value in result.items() if key not in ("stdout", "stderr")},
            **{name: {"text": result[name].decode("utf-8", errors="replace"),
                      "base64": base64.b64encode(result[name]).decode("ascii")}
               for name in ("stdout", "stderr")}}


def classify(source, bytecode):
    if source["timed_out"] or bytecode["timed_out"]:
        return "TIMEOUT"
    if source["returncode"] != 0 or bytecode["returncode"] != 0:
        # Even two identical failures do not prove bytecode compatibility.
        return "EXECUTION_FAILED"
    return "MATCH" if source["stdout"] == bytecode["stdout"] else "OUTPUT_DIFF"


def first_difference(left, right):
    return next((index for index, pair in enumerate(zip(left, right)) if pair[0] != pair[1]),
                min(len(left), len(right)) if len(left) != len(right) else None)


def export(reference, source, destination, timeout, cwd):
    result = run([reference, "--dump", source, destination], timeout, cwd)
    if result["timed_out"] or result["returncode"] != 0:
        raise RuntimeError(f"reference export failed: {encoded_run(result)}")
    if not destination.is_file() or destination.read_bytes()[:8] != MAGIC:
        raise RuntimeError("reference --dump did not produce a TJS2 100 bytecode file")


def probe_reference(reference, cial, timeout, directory):
    # Two unpredictable inputs reject binaries that ignore their script argument.
    for index in range(2):
        marker = f"cial_probe_{uuid.uuid4().hex}"
        source = directory / f"probe{index}.tjs"
        source.write_text(f'Debug.message("{marker}");', encoding="utf-8")
        result = run([reference, source], timeout, directory)
        if result["timed_out"] or result["returncode"] != 0 or marker.encode() not in result["stdout"]:
            raise RuntimeError("reference executable failed the script-path capability probe")
        destination = directory / f"probe{index}.tjsbc"
        export(reference, source, destination, timeout, directory)
        loaded = run([cial, "--tjsbc", destination], timeout, directory)
        if loaded["timed_out"] or loaded["returncode"] != 0 or marker.encode() not in loaded["stdout"]:
            raise RuntimeError("reference export failed the Ciallang execution capability probe")
    return {"executable": str(reference), "sha256": hashlib.sha256(reference.read_bytes()).hexdigest(),
            "script_path_probe": True, "export_execution_probe": True}


def compare_case(cial, source, bytecode, timeout, cwd):
    source_result = run([cial, source], timeout, cwd)
    bytecode_result = run([cial, "--tjsbc", bytecode], timeout, cwd)
    return {"status": classify(source_result, bytecode_result), "source": str(source),
            "bytecode": str(bytecode),
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "bytecode_sha256": hashlib.sha256(bytecode.read_bytes()).hexdigest(),
            "first_different_stdout_byte": first_difference(source_result["stdout"], bytecode_result["stdout"]),
            "source_run": encoded_run(source_result), "bytecode_run": encoded_run(bytecode_result)}


def execute_case(cial, bytecode, timeout, cwd):
    result = run([cial, "--tjsbc", bytecode], timeout, cwd)
    status = "TIMEOUT" if result["timed_out"] else "EXECUTED" if result["returncode"] == 0 else "EXECUTION_FAILED"
    return {"status": status, "bytecode": str(bytecode),
            "bytecode_sha256": hashlib.sha256(bytecode.read_bytes()).hexdigest(),
            "bytecode_run": encoded_run(result)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cial", required=True, type=Path)
    parser.add_argument("--mode", choices=("compare", "execute"), default="compare",
                        help="compare source/bytecode stdout, or only check bytecode execution")
    parser.add_argument("--fixture-dir", type=Path, default=ROOT / "tests/test_files/tjsbc")
    parser.add_argument("--source-dir", type=Path, default=ROOT / "tests/test_files/tjs2")
    parser.add_argument("--case", action="append", default=[], help="fixture stem; repeat to select cases")
    parser.add_argument("--reference", type=Path, help="probe this optional CLI, then export fresh bytecode")
    parser.add_argument("--report", type=Path, help="write the same JSON report to this file")
    parser.add_argument("--timeout", type=float, default=15)
    args = parser.parse_args(argv)
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if args.mode == "execute" and args.reference:
        parser.error("--reference requires --mode compare")
    args.cial = args.cial.resolve()
    if not args.cial.is_file():
        parser.error("--cial must name an executable file")
    fixtures = sorted(args.fixture_dir.resolve().glob("*.tjsbc"))
    if not fixtures:
        parser.error("fixture directory contains no .tjsbc files")
    unknown = set(args.case) - {path.stem for path in fixtures}
    if unknown:
        parser.error(f"unknown cases: {', '.join(sorted(unknown))}")
    report = {"coverage_mode": "source_vs_bytecode" if args.mode == "compare" else "bytecode_execution",
              "comparison": ("Ciallang source versus bytecode; stdout compared byte for byte" if args.mode == "compare"
                             else "Bytecode execution smoke check; no source equivalence assertion"),
              "cial": {"executable": str(args.cial),
                       "sha256": hashlib.sha256(args.cial.read_bytes()).hexdigest()},
              "reference": None, "cases": []}
    with tempfile.TemporaryDirectory(prefix="cial-bytecode-") as temporary:
        directory = Path(temporary)
        if args.reference:
            args.reference = args.reference.resolve()
            try:
                report["reference"] = probe_reference(args.reference, args.cial, args.timeout, directory)
            except (OSError, RuntimeError) as error:
                report["error"] = str(error)
        if "error" not in report:
            for fixture in fixtures:
                if args.case and fixture.stem not in args.case:
                    continue
                if args.mode == "execute":
                    case = {"name": fixture.stem}
                    try:
                        case.update(execute_case(args.cial, fixture, args.timeout, directory))
                    except (OSError, RuntimeError) as error:
                        case.update(status="TOOL_ERROR", error=str(error))
                    report["cases"].append(case)
                    continue
                source = fixture.with_suffix(".tjs")
                if not source.is_file():
                    source = args.source_dir.resolve() / f"{fixture.stem}.tjs"
                case = {"name": fixture.stem, "status": "SKIPPED_NO_SOURCE", "bytecode": str(fixture)}
                if source.is_file():
                    try:
                        bytecode = fixture
                        if args.reference:
                            bytecode = directory / fixture.name
                            export(args.reference, source, bytecode, args.timeout, directory)
                        case.update(compare_case(args.cial, source, bytecode, args.timeout, directory))
                    except (OSError, RuntimeError) as error:
                        case.update(status="TOOL_ERROR", error=str(error))
                report["cases"].append(case)
    report["summary"] = {status: sum(case["status"] == status for case in report["cases"])
                         for status in sorted({case["status"] for case in report["cases"]})}
    compared = [case for case in report["cases"] if case["status"] != "SKIPPED_NO_SOURCE"]
    success_status = "MATCH" if args.mode == "compare" else "EXECUTED"
    report["ok"] = ("error" not in report and bool(compared)
                    and all(case["status"] == success_status for case in compared))
    payload = json.dumps(report, ensure_ascii=True, indent=2)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(payload + "\n", encoding="utf-8")
    print(payload)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
