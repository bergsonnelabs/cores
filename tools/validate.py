#!/usr/bin/env python3
"""
validate.py — End-to-end SDK validation runner.

Runs coregen + compile for each test project in tests/.
Reports pass/fail, binary size, and warnings.

Usage:
  python3 tools/validate.py              # run all tests
  python3 tools/validate.py val-blink-gpio  # run one test
  python3 tools/validate.py --generate-only # coregen only, no compile
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path


# ---- Console encoding ----------------------------------------------------
# Windows Python falls back to the legacy ANSI code page (cp1252) whenever
# stdout isn't a real console — which is exactly the case under make, where it
# is a pipe. Any non-ASCII in our progress output then raises
# UnicodeEncodeError and takes the build down with it. Force UTF-8 on the
# streams we own; errors="replace" keeps a terminal that genuinely cannot
# render a character from crashing over it.
for _stream in (sys.stdout, sys.stderr):
    try:
        if (getattr(_stream, "encoding", "") or "").lower().replace("-", "") != "utf8":
            _stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError, ValueError):
        pass

SDK_ROOT = Path(__file__).parent.parent
TESTS_DIR = SDK_ROOT / "tests"
MAKE = "make"

# ARM toolchain (macOS default location)
ARM_PATH = "/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin"


def find_tests(filter_name=None):
    """Find all test directories with a config.json."""
    tests = []
    for d in sorted(TESTS_DIR.iterdir()):
        if not d.is_dir():
            continue
        if not (d / "config.json").exists():
            continue
        if filter_name and d.name != filter_name:
            continue
        with open(d / "config.json") as f:
            proj = json.load(f)
        core = proj.get("core", "?")
        # Check for negative test marker
        negative = (d / "main.c").read_text().startswith("/* NEGATIVE TEST")
        tests.append({
            "name": d.name,
            "path": d,
            "core": core,
            "negative": negative,
        })
    return tests


def run_make(project_path, target, env, capture=True):
    """Run make in a test project directory."""
    cmd = [MAKE, "-C", str(project_path), target]
    result = subprocess.run(
        cmd,
        env=env,
        capture_output=capture,
        text=True,
        timeout=120,
    )
    return result


def get_binary_size(project_path):
    """Get the binary size from the build output."""
    build_dir = project_path / "build"
    for ext in [".bin", ".elf"]:
        for f in build_dir.glob(f"*{ext}"):
            return f.stat().st_size
    return 0


def count_warnings(stderr):
    """Count compiler warnings in build output."""
    return len(re.findall(r': warning:', stderr or ""))


def main():
    parser = argparse.ArgumentParser(description="Cores SDK validation runner")
    parser.add_argument("test", nargs="?", help="Run only this test")
    parser.add_argument("--generate-only", action="store_true",
                        help="Run coregen only, skip compilation")
    parser.add_argument("--clean", action="store_true",
                        help="Clean build artifacts before running")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Show full build output")
    args = parser.parse_args()

    tests = find_tests(args.test)
    if not tests:
        print(f"No tests found{' matching ' + args.test if args.test else ''}.")
        sys.exit(1)

    # Set up environment with ARM toolchain on PATH
    env = os.environ.copy()
    env["PATH"] = ARM_PATH + ":" + env.get("PATH", "")

    print(f"\n{'='*70}")
    print(f"  Cores SDK Validation — {len(tests)} test(s)")
    print(f"{'='*70}\n")

    results = []
    start_all = time.time()

    for test in tests:
        name = test["name"]
        path = test["path"]
        core = test["core"]
        negative = test["negative"]

        print(f"  {name:<24} [{core}]", end="", flush=True)

        # Ensure Makefile exists (coregen creates it on first run)
        # We need to generate first if no Makefile
        makefile = path / "Makefile"

        # Clean if requested
        if args.clean and makefile.exists():
            run_make(path, "distclean", env)

        # Phase 1: Generate (coregen)
        gen_result = run_make(path, "generate", env)
        gen_ok = gen_result.returncode == 0

        if not gen_ok:
            print(f"  generate:FAIL")
            if args.verbose:
                print(gen_result.stderr)
            results.append({"name": name, "core": core, "generate": False,
                           "compile": False, "size": 0, "warnings": 0,
                           "negative": negative, "error": gen_result.stderr})
            continue

        if args.generate_only:
            print(f"  generate:OK")
            results.append({"name": name, "core": core, "generate": True,
                           "compile": None, "size": 0, "warnings": 0,
                           "negative": negative})
            continue

        # Phase 2: Compile
        build_result = run_make(path, "all", env)
        compile_ok = build_result.returncode == 0
        warnings = count_warnings(build_result.stderr)
        size = get_binary_size(path) if compile_ok else 0

        if negative:
            # Negative test: compilation SHOULD fail (typically #error guard)
            if not compile_ok and "error" in (build_result.stderr or "").lower():
                print(f"  generate:OK  compile:EXPECTED_FAIL  ✓")
                results.append({"name": name, "core": core, "generate": True,
                               "compile": True, "size": 0, "warnings": 0,
                               "negative": True, "passed": True})
            elif compile_ok:
                print(f"  generate:OK  compile:SHOULD_HAVE_FAILED  ✗")
                results.append({"name": name, "core": core, "generate": True,
                               "compile": False, "size": size, "warnings": warnings,
                               "negative": True, "passed": False,
                               "error": "Expected #error but compilation succeeded"})
            else:
                print(f"  generate:OK  compile:WRONG_ERROR  ✗")
                results.append({"name": name, "core": core, "generate": True,
                               "compile": False, "size": 0, "warnings": 0,
                               "negative": True, "passed": False,
                               "error": build_result.stderr[-200:] if build_result.stderr else "unknown"})
        else:
            # Positive test: compilation should succeed
            size_kb = size / 1024 if size else 0
            status = "OK" if compile_ok else "FAIL"
            warn_str = f"  {warnings} warn" if warnings else ""
            size_str = f"  {size_kb:.1f}KB" if size else ""
            symbol = "✓" if compile_ok else "✗"
            print(f"  generate:OK  compile:{status}{size_str}{warn_str}  {symbol}")
            if not compile_ok and args.verbose:
                print(build_result.stderr[-500:] if build_result.stderr else "")
            results.append({"name": name, "core": core, "generate": True,
                           "compile": compile_ok, "size": size, "warnings": warnings,
                           "negative": False, "passed": compile_ok})

    elapsed = time.time() - start_all

    # Summary
    passed = sum(1 for r in results if r.get("passed", r.get("compile")))
    failed = len(results) - passed
    total_warnings = sum(r.get("warnings", 0) for r in results)

    print(f"\n{'='*70}")
    print(f"  {passed} passed, {failed} failed, {total_warnings} warnings  ({elapsed:.1f}s)")
    if failed:
        print(f"\n  Failed:")
        for r in results:
            if not r.get("passed", r.get("compile")):
                err = r.get("error", "")
                if len(err) > 100:
                    err = err[:100] + "..."
                print(f"    {r['name']}: {err}")
    print(f"{'='*70}\n")

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
