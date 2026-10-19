#!/usr/bin/env python3
"""Source-tree inspector for the LXX public repository.

Modes
-----
  --check                 run every hygiene check, exit non-zero on findings
  --manifest [--json]     print the file manifest (path, bytes, sha256, lines)
  --denylist FILE         additionally scan for tokens listed in FILE

Denylist file format
--------------------
One entry per line.  Blank lines and lines starting with ``#`` are ignored.
A plain entry is matched as a literal, case-sensitive substring.
An entry wrapped in slashes (``/regex/``) is compiled as a regular expression.

The denylist is maintainer-side: it lives outside this repository so that the
public tree never contains the very tokens it is required to forbid.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys

# Public-safe hygiene patterns.  Anything that would itself publish a private
# or legacy marker belongs in the maintainer-side denylist instead.
PRIVATE_PATH_RES = [
    re.compile(r"[A-Za-z]:\\(Users|Windows|Program Files)"),
    re.compile(r"/home/[A-Za-z_][\w.-]*/\.(ssh|aws|gnupg|gnome-keyring)"),
    re.compile(r"/Users/[A-Za-z_][\w.-]*/"),
]

BINARY_ALLOWLIST = {
    "src/plugin/resources/Fonts/din1451alt.ttf",
}

SOURCE_EXT = {".h", ".hpp", ".hh", ".ipp", ".inl", ".c", ".cc", ".cpp", ".cxx"}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)

# Headers that only exist at build time, provided by JUCE or by
# juce_generate_juce_header().  Everything else must resolve in this tree.
BUILD_GENERATED_PREFIXES = ("juce_",)
BUILD_GENERATED_NAMES = {"JuceHeader.h"}


def walk(root: str):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(
            d for d in dirnames
            if d not in {".git", "build", "_deps", "cmake-build-debug",
                         "cmake-build-release", "__pycache__", "artifacts",
                         "test-results"}
        )
        for name in sorted(filenames):
            yield os.path.join(dirpath, name)


def sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def load_denylist(path: str):
    entries = []
    with open(path, "r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if len(line) > 2 and line.startswith("/") and line.endswith("/"):
                entries.append(("regex", re.compile(line[1:-1]), line))
            else:
                entries.append(("literal", line, line))
    return entries


def resolve_includes(root: str, rel: str, text: str):
    """Verify every quoted #include resolves inside this repository.

    Search order mirrors the build: the including file's own directory, then
    the include directories CMake exports (src/dsp is PUBLIC on lxx_dsp,
    src/plugin is PRIVATE on the plugin target), then the repository root.
    """
    findings = []
    ext = os.path.splitext(rel)[1]
    if ext not in SOURCE_EXT:
        return findings

    here = os.path.dirname(os.path.join(root, rel))
    base_dirs = [
        here,
        os.path.join(root, "src", "dsp"),
        os.path.join(root, "src", "plugin"),
        os.path.join(root, "src"),
        os.path.join(root, "tests"),
        root,
    ]

    for lineno, line in enumerate(text.splitlines(), 1):
        m = INCLUDE_RE.match(line)
        if not m:
            continue
        target = m.group(1)
        if target in BUILD_GENERATED_NAMES:
            continue
        if target.startswith(BUILD_GENERATED_PREFIXES):
            continue
        resolved = False
        for base in base_dirs:
            if os.path.isfile(os.path.normpath(os.path.join(base, target))):
                resolved = True
                break
        if not resolved:
            findings.append((rel, lineno, f'unresolved #include "{target}"'))
    return findings


def scan_text(rel: str, text: str):
    """Yield (rel, lineno, message) findings for a text file."""
    findings = []
    if text.startswith("\ufeff"):
        findings.append((rel, 1, "UTF-8 BOM present"))
    for i, line in enumerate(text.splitlines(), 1):
        if "\r" in line:
            findings.append((rel, i, "carriage return (CRLF) present"))
            break
    for i, line in enumerate(text.splitlines(), 1):
        for rx in PRIVATE_PATH_RES:
            if rx.search(line):
                findings.append((rel, i, f"private path shape: {rx.pattern}"))
    return findings


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=".", help="repository root")
    ap.add_argument("--check", action="store_true", help="run hygiene checks")
    ap.add_argument("--manifest", action="store_true", help="print file manifest")
    ap.add_argument("--json", action="store_true", help="JSON manifest output")
    ap.add_argument("--denylist", help="external denylist file")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print(f"error: {root} is not a directory", file=sys.stderr)
        return 2

    findings = []
    manifest = []
    deny_entries = load_denylist(args.denylist) if args.denylist else []

    for path in walk(root):
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        if rel in BINARY_ALLOWLIST:
            st = os.stat(path)
            manifest.append({
                "path": rel, "bytes": st.st_size, "lines": None,
                "sha256": sha256(path), "kind": "binary-allowlisted",
            })
            continue

        try:
            with open(path, "rb") as fh:
                blob = fh.read()
        except OSError as exc:
            findings.append((rel, 0, f"unreadable: {exc}"))
            continue

        if b"\x00" in blob:
            st = os.stat(path)
            findings.append((rel, 0, "binary data outside the allowlist"))
            manifest.append({
                "path": rel, "bytes": st.st_size, "lines": None,
                "sha256": hashlib.sha256(blob).hexdigest(), "kind": "binary",
            })
            continue

        try:
            text = blob.decode("utf-8")
        except UnicodeDecodeError as exc:
            findings.append((rel, 0, f"not valid UTF-8: {exc}"))
            continue

        manifest.append({
            "path": rel,
            "bytes": len(blob),
            "lines": text.count("\n") + (0 if text.endswith("\n") or not text else 1),
            "sha256": hashlib.sha256(blob).hexdigest(),
            "kind": "text",
        })

        if args.check:
            findings.extend(scan_text(rel, text))
            findings.extend(resolve_includes(root, rel, text))
            for lineno, line in enumerate(text.splitlines(), 1):
                for kind, pattern, display in deny_entries:
                    hit = (pattern.search(line) if kind == "regex"
                           else (display in line))
                    if hit:
                        findings.append(
                            (rel, lineno, f"denylist hit [{kind}] {display}")
                        )

    if args.manifest or args.json:
        if args.json:
            json.dump(sorted(manifest, key=lambda e: e["path"]),
                      sys.stdout, indent=2, sort_keys=True)
            sys.stdout.write("\n")
        else:
            total_bytes = sum(e["bytes"] for e in manifest)
            total_lines = sum(e["lines"] or 0 for e in manifest)
            for e in sorted(manifest, key=lambda e: e["path"]):
                lines = "-" if e["lines"] is None else str(e["lines"])
                print(f"{e['bytes']:>9} {lines:>7}  {e['sha256'][:16]}  {e['path']}")
            print(f"{total_bytes:>9} {total_lines:>7}  "
                  f"{len(manifest)} files")

    if not args.check:
        return 0

    if findings:
        print(f"\n{len(findings)} finding(s):", file=sys.stderr)
        for rel, lineno, msg in sorted(findings):
            where = f"{rel}:{lineno}" if lineno else rel
            print(f"  {where}: {msg}", file=sys.stderr)
        return 1

    n_text = sum(1 for e in manifest if e["kind"] == "text")
    n_bin = sum(1 for e in manifest if e["kind"] == "binary")
    n_allow = sum(1 for e in manifest if e["kind"] == "binary-allowlisted")
    print(f"source tree clean: {len(manifest)} files — "
          f"{n_text} text, {n_allow} binary allowlisted, {n_bin} unexpected binary")
    return 0


if __name__ == "__main__":
    sys.exit(main())
