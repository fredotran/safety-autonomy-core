# Safety Case Traceability Matrix

This matrix maps core hazards to implemented technical controls and executable evidence.

| Hazard ID | Hazard | Control(s) | Verification Evidence |
|---|---|---|---|
| HZ-001 | Unsafe actuation after localization degradation | PID localization staleness guard, stale-output zeroing | `tests/pid_controller_tests.cpp` (`pid.localization_stale`) |
| HZ-002 | Task scheduler overload hides missed deadlines | Watchdog/deadline checks in executor, bounded catch-up limiter diagnostics | `tests/executor_watchdog_tests.cpp` |
| HZ-003 | Runtime instability from malformed startup config | Env parser full-token validation, schema migration, strict/warning validation policy | `tests/env_loader_tests.cpp`, `tests/context_factory_tests.cpp` |
| HZ-004 | Silent loss of observability due to clipped diagnostics | Fixed-capacity event buffers + truncation flags + cumulative truncation counters | `tests/diagnostic_truncation_tests.cpp` |
| HZ-005 | Heap allocation in startup safety path causes jitter/failure | Non-allocating startup payload formatting, allocation policy tests, CI symbol audit for critical objects | `tests/no_allocation_policy_tests.cpp`, `.gitlab-ci.yml` allocator symbol gate |
| HZ-006 | Integration regressions from packaging/config drift | CMake package config export + consumer smoke test | `tests/package_config_smoke.cmake` (`package_config_smoke`) |
| HZ-007 | State-estimation divergence from bounded disturbances | Bounded EKF + complementary filter clamps and finite guards | `tests/filter_invariants_tests.cpp` |
| HZ-008 | Unsafe trajectory command violates motion limits | Trajectory validator for speed/accel/jerk/time monotonicity | `tests/motion_trajectory_tests.cpp` |
| HZ-009 | Late detection of runtime degradation | Periodic health beacon topic with watchdog/truncation telemetry | `tests/health_beacon_tests.cpp`, `docs/diagnostics/watchdog_integration_guide.md` |

## Assumptions
- Platform clock source is monotonic and stable.
- Safety build profile disables sanitizers and keeps warnings as errors.
- Runtime deployment uses a deterministic scheduler tick source.

## Open Items
- Extend traceability down to requirement IDs once formal safety requirements are frozen.
- Add MC/DC-oriented coverage evidence for safety-critical units.
