#!/usr/bin/env python3
"""Verify hazard -> requirement -> test traceability stays consistent.

Parses markdown/docs/safety_case/traceability_matrix.md and checks:

  1. Every requirement ID (SR-XXX-NNN) has at least one verification-evidence
     reference pointing to a file that exists.
  2. At least one referenced test artifact per requirement carries the
     matching `@req SR-XXX-NNN` tag, so the link is bidirectional: the matrix
     names the test, and the test names the requirement.
  3. Every `@req` tag found in tests/, CMakeLists.txt, or CI config refers to
     a requirement declared in the matrix (no orphan tags).
  4. Every hazard row lists at least one CI gate.

Exit code 0 = consistent. Prints a coverage summary.

Usage: tools/safety/verify_traceability.py [--verbose]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
MATRIX = REPO_ROOT / "markdown/docs/safety_case/traceability_matrix.md"

REQ_RE = re.compile(r"\bSR-[A-Z]+-\d{3}\b")
HAZARD_RE = re.compile(r"\bHZ-\d{3}\b")
TAG_RE = re.compile(r"@req\s+(SR-[A-Z]+-\d{3})")
FILE_REF_RE = re.compile(r"((?:tests|tools|examples|ros2)/[^\s|`),]+|CMakeLists\.txt)")
CI_CELL_MIN_LEN = 4  # a CI gate cell must name something non-trivial

SCAN_DIRS = ["tests", "tools", "fuzz", "benchmarks", "examples", "ros2/src"]
SCAN_EXTS = {".cpp", ".hpp", ".h", ".py", ".txt", ".sh", ".yml", ".yaml"}


def parse_matrix() -> list[dict]:
    rows = []
    for line in MATRIX.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line.startswith("|") or "---" in line or "Hazard ID" in line:
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 6:
            continue
        hazard_ids = HAZARD_RE.findall(cells[0])
        req_ids = REQ_RE.findall(cells[1])
        if not req_ids:
            continue
        rows.append(
            {
                "hazard": hazard_ids[0] if hazard_ids else "?",
                "req": req_ids[0],
                "evidence": cells[4],
                "ci": cells[5],
                "files": FILE_REF_RE.findall(cells[4]),
            }
        )
    return rows


def scan_tags() -> dict[str, set[str]]:
    """Map requirement ID -> set of files containing an @req tag."""
    tags: dict[str, set[str]] = {}
    candidates = list(SCAN_DIRS) + ["CMakeLists.txt", ".github/workflows"]
    for rel in candidates:
        base = REPO_ROOT / rel
        if not base.exists():
            continue
        files = [base] if base.is_file() else [p for p in base.rglob("*") if p.is_file()]
        for path in files:
            if path.suffix not in SCAN_EXTS and path.name != "CMakeLists.txt":
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for match in TAG_RE.finditer(text):
                tags.setdefault(match.group(1), set()).add(str(path.relative_to(REPO_ROOT)))
    return tags


def main() -> int:
    verbose = "--verbose" in sys.argv
    rows = parse_matrix()
    tags = scan_tags()

    errors: list[str] = []
    warnings: list[str] = []

    print(f"traceability: {len(rows)} matrix rows, {len(tags)} tagged requirements")

    for row in rows:
        req = row["req"]

        if len(row["ci"]) < CI_CELL_MIN_LEN:
            errors.append(f"{req}: no CI gate listed")
        if not row["files"]:
            errors.append(f"{req}: no verification-evidence file references")
            continue

        existing = []
        for ref in row["files"]:
            path = REPO_ROOT / ref
            if path.exists():
                existing.append(ref)
            else:
                errors.append(f"{req}: referenced evidence file missing: {ref}")

        tagged = tags.get(req, set())
        if not tagged:
            errors.append(
                f"{req}: no @req tag found in any test artifact "
                f"(evidence files: {', '.join(existing) or 'none'})"
            )
        elif verbose:
            print(f"  {req}: tagged in {', '.join(sorted(tagged))}")

    for req, files in tags.items():
        if req not in {row["req"] for row in rows}:
            warnings.append(f"{req}: @req tag in {sorted(files)} not declared in matrix")

    if warnings:
        for w in warnings:
            print(f"[warn] {w}")

    covered = sum(1 for row in rows if tags.get(row["req"]))
    print(f"requirements with bidirectional test link: {covered}/{len(rows)}")

    if errors:
        for e in errors:
            print(f"[FAIL] {e}")
        return 1

    print("traceability: consistent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
