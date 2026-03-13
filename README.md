# Safety Autonomy Core

High-assurance C++ core for safety-minded robotics and autonomous vehicles: deterministic state machines, bounded executors, diagnostics, and motion/control primitives with MISRA/AUTOSAR-inspired rules. Focused on AGV/AMR operation inside dynamic warehouses (pedestrians, forklifts, pop-up obstacles, temporary localization loss).

## Scope
- Deterministic mode/state management (Init, Standby, Active, Degraded, AvoidingObstacle, LocalizationLost, Docking, SafeStop)
- Diagnostics and health state with latched faults + pluggable health monitor/transport callbacks
- Time/budget utilities, bounded task executor with platform clocks + watchdog windows
- Safety envelope helper and config-driven motion envelope enforcement
- Deterministic safety supervisor with warning/degraded/critical escalation to SafeStop
- Controllers & filters: Safety PID, alpha-beta, complementary filter, bounded EKF (bounded state + finite guards)
- Startup context factory with environment overrides and config validation hook
- Motion trajectory primitives and validator for speed/accel/jerk/time monotonicity checks

## Repository layout
- `include/safety_core/` public headers (state machine, diagnostics, exec, control, filters, HALs)
- `src/` implementations (platform clocks, diagnostics adapters, control/filter cores)
- `tests/` lightweight unit/property tests (state machine, safety envelope, system context integration)
- `tools/dev/run_quality_gate.sh` one-command local maintainer gate (build + tests + policy, optional coverage)
- `markdown/CONTRIBUTING.md` maintenance workflow, PR checklist, and safety-case update expectations
- `CMakeLists.txt` library + smoke test target
- `.clang-format`, `.clang-tidy` house style and lint rules
- `.gitlab-ci.yml` CI pipeline (format, clang-tidy, build, tests, security scans)

## Build and test (local)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSAFETY_CORE_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Packaging
Generate installable archives (library + headers + demo app):

```bash
cmake --build build --target package
```

This produces both `.tar.gz` and `.zip` artifacts under `build/` with platform-specific names such as
`safety-core-0.1.0-Linux-x86_64.{tar.gz,zip}`. To create a source-only bundle:

```bash
cmake --build build --target package_source
```

Artifacts include the exported CMake config files so downstream projects can `find_package(safety_core CONFIG)`.

### ROS 2 package usage
This repository can be built as an `ament_cmake` ROS 2 package (`package.xml` at repo root):

```bash
colcon build --packages-select safety_autonomy_core --cmake-args -DSAFETY_CORE_ENABLE_AMENT=ON
```

After sourcing the workspace, downstream ROS 2 packages can link the library with:

```cmake
find_package(safety_autonomy_core REQUIRED)
target_link_libraries(my_node PRIVATE safety_core::safety_core)
```

If you prefer non-ROS standalone builds, keep `SAFETY_CORE_ENABLE_AMENT=OFF` (default).

## Maintainer workflow (recommended)
```bash
tools/dev/run_quality_gate.sh
```

Optional (includes coverage gate):

```bash
tools/dev/run_quality_gate.sh --coverage
```

## Build profiles (presets)
Two baseline build profiles are available via `CMakePresets.json`:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

cmake --preset safety
cmake --build --preset safety
ctest --preset safety

cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
```

- `dev`: `RelWithDebInfo` + sanitizers + `-Werror`
- `safety`: `Release` + sanitizers disabled + `-Werror`
- `coverage`: `Debug` + coverage instrumentation + `-Werror`

## Git hooks
Enable repository-managed hooks:

```bash
git config core.hooksPath .githooks
```

`pre-commit` behavior:
- auto-formats staged C/C++ files with `clang-format -i`
- re-stages formatted files automatically
- runs `clang-tidy` on staged C++ files (requires `build/compile_commands.json`)

Set `AUTO_FIX_FORMAT=0` to switch pre-commit back to check-only formatting mode.
Set `SKIP_CLANG_TIDY=1` for environments where formatting checks should run but clang-tidy is intentionally skipped.

## Startup flow (env + migration + validation)
Use `system::build_context(...)` to centralize startup configuration:

1. Start from your default `config::SystemConfig`.
2. Apply environment overrides via `config::load_from_env`.
3. Migrate legacy schema versions via `config::migrate_to_current`.
4. Validate the resulting config via `config::validate`.
   - `ValidationPolicy::Strict` rejects warning-grade findings.
   - `ValidationPolicy::AllowWarnings` accepts warning-grade findings.
5. Build a ready-to-wire `SystemContext` containing config, clock, monitor, diagnostic transport, and optional `safety::SafetySupervisor` wiring.

Environment variable notes:
- `SAFETY_CORE_CONFIG_VERSION` can be used to pin the incoming config schema version.
- Unsupported future versions are rejected during startup.
- Malformed numeric env values (partial tokens, non-finite values, out-of-range integers) are ignored and fallback to defaults.

This flow is implemented in:
- `include/safety_core/system/context_factory.hpp`
- `src/system/context_factory.cpp`

Example usage is covered in `tests/context_factory_tests.cpp`.

## Diagnostics integration
- `sm::ModeStateMachine` publishes transition/fault events through `diag::DiagnosticTransport`.
- `control::SafetyPidController` publishes:
  - `pid.localization_stale`
  - `pid.output`
- `exec::TaskExecutor` publishes:
  - `executor.task_added`
  - `executor.task_run`
  - `executor.task_error`
  - `executor.deadline_miss`
  - `executor.watchdog_exceeded`
  - `executor.catch_up_limited`
- `diag::HealthBeaconPublisher` publishes:
  - `health.beacon`
- `safety::SafetySupervisor` publishes:
  - `safety.monitor_warning`
  - `safety.degraded_request`
  - `safety.safestop_forced`

Implementation notes:
- `diag::DiagnosticEvent` uses fixed-capacity topic/payload buffers (no heap allocation in event payload transport path).
- `diag::DiagnosticEvent` exposes `topic_truncated`/`payload_truncated` flags so observability pipelines can detect clipped events.
- `diag::LoggingDiagnosticTransport` tracks cumulative truncation counters (`topic_truncation_count`, `payload_truncation_count`) for trend-based alerting.
- `diag::HealthBeaconPublisher` emits periodic watchdog/truncation telemetry with fixed-capacity payloads.
- Diagnostic timestamps are derived from each module's configured `platform::Clock` when available.
- Health-monitor logs include timestamps and can use an injected clock via `diag::LoggingHealthMonitor`.
- `system::build_context` emits startup diagnostics for:
  - `config.migration`
  - `config.validation_warning`
- Diagnostic topic names are centralized in `include/safety_core/diag/topics.hpp`.

## Motion trajectory validator notes
- `motion::TrajectoryValidator` enforces finite inputs, monotonic time/distance, speed/accel/jerk bounds, and optional stopping-envelope checks.
- `TrajectoryViolation::NullInput` is used for null trajectory buffers and non-finite sample fields.

## Allocation-aware executor callback API
`TaskExecutor` uses function-pointer callbacks with opaque context to avoid `std::function` allocations in scheduling paths:

```cpp
using TaskFn = Result (*)(time::TimePoint, void*) noexcept;
executor.add_task(task_fn, task_context, period);
```

Optional scheduler catch-up policy:

```cpp
executor.set_catch_up_policy(exec::CatchUpPolicy::SingleStep);
executor.set_catch_up_policy(exec::CatchUpPolicy::BoundedCatchUp, 2U);
```

Use `diag::LoggingDiagnosticTransport` for host-side observability and bring your own transport implementation for embedded/production paths.

Notes:
- Sanitizers are enabled by default for non-safety builds; disable with `-DSAFETY_CORE_ENABLE_SANITIZERS=OFF` when targeting production-like safety builds.
- Warnings are treated as errors by default; toggle with `-DSAFETY_CORE_ENABLE_WERROR=OFF` if bootstrapping a new toolchain.

## Coding standards (starter set)
- MISRA/AUTOSAR-inspired: no dynamic allocation in RT paths, no exceptions/RTTI in core, fixed-capacity containers, explicit initialization.
- Strong typing and units/frames; [[nodiscard]] for status-returning APIs.
- Saturation/NaN/Inf guards at boundaries; deterministic math (no fast-math).
- Concurrency: bounded queues only; timed waits; avoid unbounded locks.

## CI pipeline
- `format_check`: clang-format guard on `include/`, `src/`, `tests/`
- `clang_tidy`: static analysis over library sources
- `hook_smoke`: validates repository pre-commit auto-fix behavior on staged C++ files
- `policy_guard`: banned API checks (exceptions, dynamic allocation calls, `abort`) in core safety code
- `safety_case_guard`: verifies required safety-case artifacts and release-checklist structure are present
- `build_and_test`: CMake build + ctest (with sanitizers on by default)
- Includes policy tests that guard no-allocation startup paths (`no_allocation_policy_tests`).
- Includes a symbol-level guard that fails CI if `context_factory` object code references heap allocation APIs (`operator new`/`malloc` family).
- Extends symbol-level no-allocation guards to `task_executor`, `safety_pid`, `state_machine`, `trajectory`, `bounded_ekf_filter`, `complementary_filter`, and `safety_supervisor` objects.
- `coverage`: GCC/gcovr coverage gate using CI variables:
  - `COVERAGE_MIN_LINE` (default `80`)
  - `COVERAGE_MIN_BRANCH` (default `55`)
- Security templates: GitLab SAST + Secret Detection

## Test coverage highlights
- `tests/state_machine_tests.cpp`: transition and fault-latch behavior.
- `tests/pid_controller_tests.cpp`: localization timeout guard, clamp boundaries, PID diagnostic events.
- `tests/executor_watchdog_tests.cpp`: watchdog and deadline miss regression checks + executor diagnostics.
- `tests/deterministic_replay_tests.cpp`: fixed-seed deterministic replay of executor diagnostics/event ordering.
- `tests/filter_invariants_tests.cpp`: bounded EKF + complementary filter invariant/property checks.
- `tests/motion_trajectory_tests.cpp`: emergency-stop primitive generation + trajectory validator safety checks.
- `tests/health_beacon_tests.cpp`: beacon cadence and payload assertions.
- `tests/fault_injection_tests.cpp`: backward-clock/transport-drop/NaN-burst robustness checks.
- `tests/safety_supervisor_tests.cpp`: deterministic warning/degraded/critical monitor escalation, localization-age and diagnostic-drop monitor checks, setter wiring, and safe-stop forcing.
- `tests/safety_envelope_tests.cpp`: scenario checks and property-style monotonic boundary checks.
- `tests/context_factory_tests.cpp`: startup env override + validation-hook integration, optional supervisor pointer wiring, strict-policy failure guarantees, and schema/policy matrix checks.
- `tests/diagnostic_truncation_tests.cpp`: forced topic/payload clipping and truncation observability flags.
- `tests/no_allocation_policy_tests.cpp`: verifies startup build-context path avoids heap allocations.
- `package_config_smoke`: verifies install/export + `find_package(safety_core CONFIG)` consumption.

## Safety case artifacts
- `markdown/docs/safety_case/traceability_matrix.md`: hazard-to-control-to-test traceability starter matrix.
- `markdown/docs/safety_case/outline.md`: structured claims/assumptions/residual-risk starter outline.
- `markdown/docs/safety_case/assumptions_register.md`: explicit assumption inventory and monitoring evidence.
- `markdown/docs/safety_case/residual_risk_register.md`: residual risk tracking with mitigation ownership.
- `markdown/docs/safety_case/safety_manual.md`: integration constraints and required wiring for safety use.
- `markdown/docs/safety_case/verification_protocol_template.md`: requirement-driven verification and sign-off template.
- `markdown/docs/safety_case/iso_3691_4_clause_matrix.md`: clause-level conformance starter matrix with explicit certification-gap tracking.
- `markdown/docs/safety_case/iso26262_iso13849_crosswalk.md`: repository-level crosswalk of expected ISO 26262/ISO 13849 evidence domains.
- `markdown/docs/safety_case/functional_safety_plan.md`: functional safety activity, role, and release-gate plan.
- `markdown/docs/safety_case/vv_strategy.md`: verification and validation strategy baseline for safety requirements.
- `markdown/docs/safety_case/tool_confidence_register.md`: tool confidence controls for safety evidence toolchain.
- `markdown/docs/safety_case/change_impact_assessment_template.md`: template for safety change-impact evaluation.
- `markdown/docs/safety_case/release_safety_evidence_checklist.md`: release gating checklist for safety evidence completeness.

### Standards readiness note
This repository includes process and evidence scaffolding intended to support ISO 26262 and ISO 13849 readiness activities.
Formal compliance/certification claims still require project-specific system integration evidence, independent assessment, and organizational process records.

## Diagnostics integration guide
- `markdown/docs/diagnostics/watchdog_integration_guide.md`: watchdog sizing, required topics, and beacon wiring checklist.

## License
TBD (select per-robot/per-project commercial licensing with mandatory support/maintenance).
