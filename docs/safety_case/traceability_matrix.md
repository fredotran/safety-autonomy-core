# Safety Case Traceability Matrix

This matrix maps hazards to requirements, controls, executable evidence, and CI gate coverage.

| Hazard ID | Requirement ID | Hazard | Control(s) | Verification Evidence | CI Gates |
|---|---|---|---|---|---|
| HZ-001 | SR-CTRL-001 | Unsafe actuation after localization degradation | PID localization staleness guard, stale-output zeroing | `tests/pid_controller_tests.cpp` (`pid.localization_stale`) | `build_and_test`, `coverage` |
| HZ-002 | SR-EXEC-001 | Task scheduler overload hides missed deadlines | Watchdog/deadline checks in executor, bounded catch-up limiter diagnostics | `tests/executor_watchdog_tests.cpp` | `build_and_test`, `coverage` |
| HZ-003 | SR-CONFIG-001 | Runtime instability from malformed startup config | Env parser full-token validation, schema migration, strict/warning validation policy | `tests/env_loader_tests.cpp`, `tests/context_factory_tests.cpp` | `build_and_test`, `coverage` |
| HZ-004 | SR-DIAG-001 | Silent loss of observability due to clipped diagnostics | Fixed-capacity event buffers + truncation flags + cumulative truncation counters | `tests/diagnostic_truncation_tests.cpp` | `build_and_test` |
| HZ-005 | SR-MEM-001 | Heap allocation in startup safety path causes jitter/failure | Non-allocating startup payload formatting, allocation policy tests, CI symbol audit for critical objects | `tests/no_allocation_policy_tests.cpp` | `build_and_test` (allocator symbol gate) |
| HZ-006 | SR-PKG-001 | Integration regressions from packaging/config drift | CMake package config export + consumer smoke test | `package_install_smoke`, `package_config_smoke` | `build_and_test` |
| HZ-007 | SR-FILTER-001 | State-estimation divergence from bounded disturbances | Bounded EKF + complementary filter clamps and finite guards | `tests/filter_invariants_tests.cpp`, `tests/fault_injection_tests.cpp` | `build_and_test`, `coverage` |
| HZ-008 | SR-MOTION-001 | Unsafe trajectory command violates motion limits | Trajectory validator for speed/accel/jerk/time monotonicity | `tests/motion_trajectory_tests.cpp` | `build_and_test`, `coverage` |
| HZ-009 | SR-DIAG-002 | Late detection of runtime degradation | Periodic health beacon topic with watchdog/truncation telemetry | `tests/health_beacon_tests.cpp`, `tests/system_context_tests.cpp` | `build_and_test` |

## Assumptions
- Platform clock source is monotonic and stable.
- Safety build profile disables sanitizers and keeps warnings as errors.
- Runtime deployment uses a deterministic scheduler tick source.

## Open Items
- Extend traceability down to requirement IDs once formal safety requirements are frozen.
- Add MC/DC-oriented coverage evidence for safety-critical units.
