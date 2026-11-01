# MISRA / Static-Analysis Coverage Status

## Current open-source coverage

| Tool | Scope | Status |
|---|---|---|
| cppcheck `--enable=all` | undefined behavior, portability, style | blocking via `static_analysis_gate.sh` (36-finding baseline ratchet) |
| clang-tidy | bugprone-*, cert-*, misc-*, performance-*, readability-*, concurrency-* | blocking via same gate |
| clang `-Weverything`-adjacent warnings + `-Werror` (certifiable target) | compile-time hygiene | blocking |
| ASan+UBSan builds | runtime UB | CI build_and_test sanitizer variant |
| libFuzzer harnesses | parser/filter crash-freedom | CI fuzz_smoke |

## Honest gaps vs. full MISRA C++:2023

Open-source tools approximate but do **not** certify MISRA compliance:

- cppcheck's `misra` addon requires the licensed `misra.txt` rule
  distribution — not shipped, not legally redistributable.
- clang-tidy `cert-*` checks cover a subset of CERT which overlaps ~60-70%
  of MISRA C++ categories (see `misra_compliance_tools.md` estimate).
- No tool today verifies the *undecidable* rules (e.g., required MC/DC
  coverage, some advisory rules).

An opt-in stricter profile ships as `.clang-tidy-misra` at the repository
root (MISRA C++:2023-flavored checks on top of the base `.clang-tidy`). It is
deliberately not wired into the blocking gate: its findings exceed the
ratchet baseline and are review material, not merge criteria. Run it
manually with `clang-tidy --config-file .clang-tidy-misra <file>` or via
`run-clang-tidy -config-file=.clang-tidy-misra`.

## What we do instead (defensible open-source posture)

1. Keep the baseline ratchet strict — **zero new findings** is enforced in CI.
2. `-fno-exceptions -fno-rtti` on the certifiable target enforces several
   MISRA categories at compile time (exceptions, RTTI).
3. `-Wconversion -Wsign-conversion -Wold-style-cast -Wdouble-promotion`
   cover numeric-cast/casting MISRA families.
4. Document every known accepted finding in `tools/ci/baselines/`.

## Deferred: commercial tooling (RR-006)

PC-lint Plus / Polyspace / QAC (~$77k–$350k+/yr per `misra_compliance_tools.md`)
remains unjustified for a 0.1.x project without a certification customer.
Tracked as accepted gap RR-006; re-evaluate on a real deployment contract.
