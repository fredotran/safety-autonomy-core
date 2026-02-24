# Safety Assumptions Register

## Purpose
This register captures operating assumptions that must remain true for safety claims and test evidence to be valid.

| Assumption ID | Statement | Rationale | Verification / Monitoring |
|---|---|---|---|
| AS-001 | Time base used by control and supervision is monotonic. | Deadline and watchdog logic depend on non-regressing time. | `tests/fault_injection_tests.cpp` clock regression case + platform clock integration tests. |
| AS-002 | Vehicle-level braking and actuation limits are consistent with configured envelope values. | Trajectory and stopping envelope checks rely on physically valid limits. | Commissioning checklist + `tests/motion_trajectory_tests.cpp` envelope checks. |
| AS-003 | Diagnostic transport integration is bounded and non-blocking for runtime threads. | Observability must not introduce unbounded jitter or deadlocks. | Platform integration review + transport drop monitoring in supervisor. |
| AS-004 | Localization update stream health is available to supervisor and controller components. | Localization stale handling requires timely freshness signals. | `tests/pid_controller_tests.cpp` + supervisor localization-age escalation tests. |
| AS-005 | Safety build profile and CI gates are enforced in release workflow. | Safety evidence is only valid if guarded build/test policies are not bypassed. | `.gitlab-ci.yml` `policy_guard`, `build_and_test`, `coverage` stages. |

## Change Control
- Any assumption invalidation requires safety case re-evaluation.
- Assumption updates must include linked test/doc changes in the same review.
