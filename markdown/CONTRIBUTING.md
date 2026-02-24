# Contributing to Safety Autonomy Core

This repository targets safety-minded, deterministic C++ behavior. Keep changes small, test-backed, and easy to review.

## 1) Development principles
- Prefer minimal changes over broad refactors.
- Preserve deterministic behavior and bounded runtime paths.
- Avoid dynamic allocation in core runtime paths.
- Keep code clang-format clean and warning-free.

## 2) Local setup
```bash
git config core.hooksPath .githooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSAFETY_CORE_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 3) One-command quality gate
Use the repository quality gate script before opening a merge request:

```bash
tools/dev/run_quality_gate.sh
```

Optional coverage gate:

```bash
tools/dev/run_quality_gate.sh --coverage
```

## 4) Code change expectations
1. Add or update tests for behavior changes.
2. Keep interfaces stable unless a clear migration path is documented.
3. Add short commentary only for non-obvious safety logic.
4. Avoid introducing new dependencies unless justified.

## 5) Pull request checklist
- [ ] Build succeeds locally.
- [ ] Relevant tests pass locally.
- [ ] Policy guard passes (`tools/safety/verify_policy.sh`).
- [ ] Docs updated when behavior, CI, or APIs changed.
- [ ] No generated artifacts committed (`*.gcov`, coverage build outputs, etc.).

## 6) Safety-case touchpoints
When adding or modifying safety behavior, update relevant docs under `docs/safety_case/`:
- `traceability_matrix.md`
- `assumptions_register.md`
- `residual_risk_register.md`
- `safety_manual.md`

## 7) Commit hygiene
- Keep commits scoped to one concern when possible.
- Use descriptive messages (`module: change summary`).
- Do not bundle formatting-only edits with behavior changes unless necessary.
