# Improvement Plan — safety-autonomy-core

Working plan produced 2026-10-31. **All 12 items are now implemented** —
items 1-3 in a first session, items 4-12 in a second (see "Done" tables
below). This file keeps the for/against assessments and records outcomes.

## Guiding theme

The project already has the right architecture and intentions. What is missing
is that guarantees be **enforced rather than declared**: blocking static
analysis, real coverage of the ROS 2 layer, living traceability, measured
determinism.

## Phase 1 — Consolidate "safety-critical" credibility

### 4. Raise branch coverage gate (55% → 75-80%)

- **For**: 55% branch coverage is incompatible with the project's claims;
  safety supervision code typically targets MC/DC or >80% branches.
- **Against**: branch testing (error paths, boundary conditions) is tedious;
  some defensive branches (`if nan` on impossible data) are expensive to reach.
- **Approach**: stepwise ratcheting (55 → 65 → 75) starting with `safety/` and
  `state_machine/`. The `COVERAGE_MIN_BRANCH` env var in `ci.yml` is the knob.

### 5. Property-based / fuzz testing on filters and parsers

- **For**: finds edge cases no human writes (NaN, non-monotonic timestamps,
  overflows); consistent with the "saturation/NaN guards at all boundaries"
  philosophy. Existing fault-injection tests are a good base.
- **Against**: CMake integration + extra CI time; fuzz harnesses must respect
  the no-allocation policy — delicate for a no-heap library.
- **Priority**: medium-high, starting with `config::load_from_env` and sensor
  input paths.

### 6. Worst-case timing benchmarks to back "deterministic"

- **For**: turns a marketing claim into evidence (p99/pmax per executor task,
  supervisor escalation latency); required material in a safety case.
- **Against**: serious WCET measurement needs target hardware and methodology;
  cloud CI only gives noisy orders of magnitude.
- **Approach**: start with Catch2/Google Benchmark microbenchmarks in CI with
  loose regression budgets; publish the numbers in the docs.

### 7. Automated hazard → code → test traceability

- **For**: `traceability_matrix.md` is manual today; a script verifying each
  requirement has a tagged test (e.g. `@req SRS-042` in gtest names/comments)
  turns the safety case from scaffolding into a living artifact. Strong
  differentiator.
- **Against**: requires tagging discipline in every test; tooling to build
  (CI regex check or small custom tool).
- **Priority**: medium — high credibility gain for moderate cost.

## Phase 2 — System robustness

### 8. Simulation non-regression tests with assertions

- **For**: `demo_tests` in CI launches demos but asserts little; converting
  scenarios (pop-up obstacle, localization loss) into tests with measurable
  assertions (zone reached, SafeStop in <N ms) catches behavioral regressions
  unit tests miss.
- **Against**: Gazebo in CI is inherently flaky (timing, headless rendering);
  SDF world maintenance.
- **Risk**: medium — mitigated by logging metrics and asserting on wide
  thresholds.

### 9. Dependency pinning + SBOM

- **For**: build reproducibility; SBOM is a standard ask for auditable
  software supply chains; `syft`/`cyclonedx` produce one in a single CI step.
- **Against**: apt/Docker pinning needs maintenance when base images move;
  low day-to-day visible value.
- **Priority**: medium, low effort.

### 10. Hardware-in-the-loop (HIL) strategy

- **For**: the true value of a safety library is proven on target (real-time,
  real drivers); even a minimal SIL→HIL loop raises credibility.
- **Against**: **largest investment** — hardware, infrastructure, flakiness;
  out of scope while the project remains a library/demo.
- **Recommendation**: only if targeting real deployment; otherwise document
  the absence as an accepted limitation in the residual risk register.

## Phase 3 — Certification (long term, costly)

### 11. Commercial MISRA tooling

- **For**: own docs estimate ~60-70% coverage via open-source tools;
  Coverity/QAC is mandatory for a certifiable claim.
- **Against**: $77k-$350k+/year per `misra_compliance_tools.md`; unjustifiable
  at 0.1.x without a certified customer/use case.
- **Verdict**: **defer** — keep the documentation scaffolding; reinvest when a
  commercial use case emerges.

### 12. Build partition: certifiable safety core vs the rest

Compile `safety_core` as a minimal certifiable unit (no Eigen, no ROS), the
rest non-certified.

- **For**: reduces certification scope ~10x; the split already exists
  conceptually (core vs ros2/).
- **Against**: Eigen is already in the core (`bounded_ekf_filter`) — removing
  or qualifying it is real work; moderate refactor effort.
- **Priority**: to evaluate — depends on the certification strategy pursued.

## Done (2026-10-31)

| # | Item | Outcome |
|---|------|---------|
| 1 | README dead links | Created DEMO_GUIDE.md, DEMO_TROUBLESHOOTING.md, ros2/RVIZ_FIX_GUIDE.md, ros2/WHEEL_ODOMETRY_NAVIGATION.md; removed COMMERCIAL_LICENSE.md ref; fixed LICENSE description |
| 2 | Blocking static analysis | `tools/ci/static_analysis_gate.sh` ratchet + committed baseline (36 tolerated findings); `continue-on-error` removed from CI |
| 3 | ROS 2 test infrastructure | gtest suites for safety_core_ros (6 files), safety_core_nav2, safety_core_msgs; pytest validation for safety_core_bringup + safety_core_sim — **106 tests, all passing in the Docker container** |

## Done — items 4-12 (second session)

| # | Item | Outcome |
|---|------|---------|
| 4 | Branch coverage | 60.6% → **75.0%** measured (lines 83.8→91.7%); `COVERAGE_MIN_BRANCH` 55→70; new `coverage_gaps_tests.cpp` + extended `state_machine_tests.cpp` |
| 5 | Property/fuzz testing | `property_tests.cpp` (~20k iterations: PID/envelope/filter invariants); libFuzzer harnesses `fuzz_env_loader` + `fuzz_filters` under `SAFETY_CORE_ENABLE_FUZZING` (~1M and ~10M runs clean); CI `fuzz_smoke` job |
| 6 | Timing evidence | `benchmarks/safety_core_benchmarks.cpp` — 8 ops, p50/p99/max + budgets; CI `benchmarks` job uploads `benchmark_results.json`; numbers published in `timing_evidence.md` (e.g. critical→SafeStop p99=437ns, max 3.7µs) |
| 7 | Traceability | `tools/safety/verify_traceability.py` — bidirectional check matrix↔`@req` tags: **10/10 requirements linked**; wired into CI `safety_case_guard` |
| 8 | Simulation regression | `test_safety_scenario.cpp` — both real nodes in-process: clear→Moving, **pop-up obstacle→latched SafeStop in 50ms**, protective hold→auto-recovery, warning never latches. 4/4 passing; suite now **133 tests** |
| 9 | Pinning + SBOM | `tools/ci/generate_sbom.sh` (syft → SPDX/CycloneDX; fallback deterministic manifest); `dependency_locks.md`; CI `sbom` job publishes artifact |
| 10 | HIL | `hil_strategy.md` — SIL→HIL ladder + minimal-viable-HIL spec; absence recorded as accepted risk **RR-005** |
| 11 | MISRA | `misra_status.md` — honest gap analysis vs MISRA C++:2023 + enforced open-source posture; commercial tooling deferred as **RR-006**; `-fno-exceptions/-fno-rtti` now compile-enforced via item 12 |
| 12 | Build partition | Dead Eigen dep **removed** (no source included it — the `FATAL_ERROR` was pure friction); `SAFETY_CORE_SOURCES` factored; `safety_core_certifiable` target compiles identical sources under `-fno-exceptions -fno-rtti -Werror`; `certifiable_partition.md` + CI `certifiable_build` job |

## Real bugs found while implementing items 4-12

1. **NaN poisoning in `safety_pid` and `alpha_beta_filter`** (item 5):
   neither guarded non-finite inputs — one NaN measurement permanently
   corrupted controller/filter state. Both now reject non-finite input with
   a diagnostic event, matching the project's own "NaN guards at all
   boundaries" claim.
2. **Fault code was unobservable end-to-end** (item 8):
   `SafetyState.fault_code` was hardwired to `0U` ("Library does not
   currently expose code") because `ModeStateMachine` had no getter. Added
   `fault_code()`; the supervisor now reports the real code (`0xE001`
   envelope emergency vs `0x03EA` localization staleness — the scenario
   test distinguishes them).
3. **`generate_sbom.sh`** had two missing `echo` keywords caught by a live
   run — fixed before commit.
4. **Publication-order race in `latch_fault`** (audit follow-up): the
   latched flag was release-stored BEFORE `fault_code_` was written, and
   `fault_code_` was a plain member while its siblings are atomics — a
   concurrent reader could observe `fault_latched()==true` with a stale
   code. `fault_code_` is now `std::atomic` and the code is stored before
   the release-store of the flag.
5. ~~**Known gap (not yet fixed)**~~ → **FIXED (staleness hardening)**:
   `SafetyEnvelopeNode` now rejects scans whose stamp is absent or older
   than `scan_timeout_s` (0.5 s default) and runs a watchdog timer that
   publishes an EMERGENCY status when the stream freezes, goes silent, or
   never starts (post-grace). `SafetySupervisorNode` independently tracks
   `safety/envelope_status` staleness via `envelope_timeout_s` (1.0 s) and
   latches fault `0xE002` → SafeStop, covering the envelope node's own
   death. Three scenario tests verify all three failure modes.
6. **Inconsistent SafetyState publication** (found while testing item 5):
   `timer_tick` cached `machine_->mode()` BEFORE the staleness checks, so
   a latch inside the same tick published `{mode=Moving, fault_latched=true}`
   — a self-contradictory SafetyState. Now re-reads the mode at publish
   time.
7. **Odometry staleness holes** (same family as item 5, fixed after plan
   review): (a) the envelope trusted `latest_speed_mps_` forever — a frozen
   odom at 0 m/s *underestimated* stopping distance; it now falls back to
   `max_speed_mps` (worst case) once `odom_timeout_s` elapses, respecting
   the startup grace for never-seen odometry; (b) the supervisor's
   localization check silently returned when no odom had *ever* arrived —
   dead-from-boot encoders now escalate from the startup baseline to
   `LocalizationLost`. Two scenario tests cover both modes.

## Real bugs found while implementing item 3

The new tests immediately caught two genuine defects:

1. **`-ffast-math` disabled every NaN/Inf guard in `safety_core_ros`** (now
   removed in its CMakeLists). `-ffinite-math-only` makes `std::isfinite(inf)`
   constant-true, so a LiDAR returning no valid points produced
   `distance_to_obstacle_m = inf`, `zone = CLEAR`, `within_envelope = true`
   instead of `-1 / WARNING`. A sensor dropout would have been reported as a
   clear path. This also contradicted the core library's own "Deterministic
   math (no -ffast-math)" standard.
2. **`agv_warehouse.launch.py` was broken at three levels** — none of which
   the old CI could see because no test ever executed the launch:
   - `environment`/`ekf` referenced as bare Python names instead of
     `LaunchConfiguration` → `generate_launch_description()` raised
     `NameError` at load time;
   - `use_ekf_env = PythonExpression([use_ekf])` evaluated the bare literal
     `true`/`false` → `NameError` at runtime;
   - `declare_environment`/`declare_map_file` missing from the returned
     `LaunchDescription` → `launch configuration 'environment' does not
     exist` at runtime.
   After the fix, a real launch was smoke-tested in the container: sim
   bridge, robot_state_publisher, the three safety nodes, and the full Nav2
   stack all start and shut down cleanly.
