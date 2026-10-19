#!/usr/bin/env python3
"""Run clang-tidy over a compile database and fail only on hard errors.

Warnings are reported as output (and as GitHub annotations when running in
Actions) but do not fail the job; a file that clang-tidy cannot analyse at all
does fail it.  That keeps this gate useful for correctness without turning it
into a style argument.

Usage: run_clang_tidy.py <path/to/compile_commands.json>
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys

ERROR_RE = re.compile(r"^(?P<file>[^\s:]+):(?P<line>\d+):(?P<col>\d+): "
                      r"(?P<kind>error|fatal error):\s*(?P<msg>.*)$")


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2

    db_path = os.path.abspath(sys.argv[1])
    if not os.path.isfile(db_path):
        print(f"compile database not found: {db_path}", file=sys.stderr)
        return 2

    if shutil.which("clang-tidy") is None:
        print("clang-tidy is not installed", file=sys.stderr)
        return 2

    with open(db_path, "r", encoding="utf-8") as fh:
        entries = json.load(fh)

    # Analyse first-party translation units only, once each.
    seen: dict[str, dict] = {}
    for entry in entries:
        src = entry.get("file", "")
        if not src:
            continue
        if not any(part in src for part in ("/src/dsp/", "/src/plugin/", "/tests/")):
            continue
        seen.setdefault(src, entry)
    if not seen:
        print("no first-party translation units in the compile database",
              file=sys.stderr)
        return 2

    root = os.path.dirname(db_path)
    warnings = 0
    failures: list[str] = []

    for src in sorted(seen):
        # `-p` makes clang-tidy use the database's include paths, defines and
        # standard, so no extra flags are forwarded here.
        cmd = ["clang-tidy", "-p", root, "--quiet", src]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
        except subprocess.TimeoutExpired:
            failures.append(f"{src}: clang-tidy timed out")
            continue

        output = (proc.stdout or "") + (proc.stderr or "")
        for line in output.splitlines():
            m = ERROR_RE.match(line.strip())
            if m:
                kind = m.group("kind")
                where = f"{m.group('file')}:{m.group('line')}:{m.group('col')}"
                if kind.endswith("error"):
                    failures.append(f"{where}: {m.group('msg')}")
                else:
                    warnings += 1
                print(line)
            elif line.strip():
                warnings += 1
                print(line)

        if proc.returncode != 0 and not any(
            f.startswith(src.split(":")[0]) for f in failures
        ):
            failures.append(
                f"{src}: clang-tidy exited with {proc.returncode}"
            )

    print(f"\nclang-tidy: {len(seen)} files, "
          f"{warnings} advisory lines, {len(failures)} hard errors")

    in_actions = os.environ.get("GITHUB_ACTIONS") == "true"
    if in_actions:
        for f in failures:
            print(f"::error::{f}")

    if failures:
        for f in failures:
            print(f"FAIL {f}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
