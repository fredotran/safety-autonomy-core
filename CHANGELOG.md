# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Setup Demo Script**: `setup_demo.sh` convenience script at repository root for automated ROS 2 workspace build and launch with interactive menu.
- **LICENSE File**: Added proprietary commercial license with per-robot/per-project licensing terms.
- **CI Improvements**: Enhanced GitLab CI with:
  - Dependency caching (apt-cache, ccache) for faster builds
  - ROS 2 build, lint, and test jobs (Jazzy distro)
  - Extended format check to include ROS 2 source files
  - Cache template for all build jobs
- **ROS 2 wrapper workspace** (`ros2/`): full ROS 2 Jazzy integration with five new packages.
  - `safety_core_msgs`: ROS 2 message definitions (`SafetyState`, `SafetyZone`, `EnvelopeStatus`, `DiagnosticEvent`, `MonitorEvent`).
  - `safety_core_ros`: wrapper nodes -- `safety_envelope_node` (lidar → zone classification), `safety_supervisor_node` (state machine + supervisor), `safety_drive_bridge_node` (Nav2 → /cmd_vel gating with jerk-limited stop and freshness watchdog).
  - `safety_core_nav2`: Nav2 behavior-tree plugin (`IsSafe` condition node) for gating motion behind safety state.
  - `safety_core_sim`: Gazebo Harmonic AGV (URDF xacro: diff drive + 360° lidar + IMU), self-contained industrial warehouse SDF (shelves, pallets, forklift, animated worker), `ros_gz_bridge` config, sim-only launch.
  - `safety_core_bringup`: top-level launch (Gazebo + Nav2 + SLAM Toolbox + RViz), Nav2 params (MPPI controller, collision_monitor zones), SLAM Toolbox params, RViz config.
- **rclcpp adapters** for safety_core abstractions: `RosClock`, `RosDiagnosticTransport`, `RosHealthMonitor`.
- **PID Anti-Windup**: Integral clamping with configurable `max_integral` limit and back-calculation anti-windup when output saturates.
- **PID Feedforward**: New `compute(setpoint, measurement, ff_velocity, ff_acceleration)` overload for trajectory tracking with velocity/acceleration feedforward terms.
- **Drive Command Freshness Watchdog**: `is_command_fresh(now_ns, max_age_ns)` on `DriveActuator` interface for detecting stale commands; `command_count()` on `BufferedDriveActuator`.
- **Thread-Safe State Machine**: `mode_` and `latched_fault_` converted to `std::atomic` with appropriate memory ordering (acquire/release) for lock-free concurrent reads.
- **Timestamped Filter Updates**: All three filters (BoundedEKF, complementary, alpha-beta) accept explicit `timestamp_ns` parameters and compute actual dt from consecutive samples.
- **Innovation Gating (Mahalanobis)**: BoundedEKF rejects measurements where `innovation^2 / S > gate_sigma^2` (default 3-sigma). Exposes `rejected_count()` for observability.
- **6-DOF IMU Sample**: `ImuSample` expanded with 3-axis angular velocity and 3-axis linear acceleration fields.
- **Multi-Zone Safety Envelope**: `SafetyZone` enum (Clear/Warning/Protective/Emergency) with `ZoneThresholds`, `RobotFootprint` geometry, and `evaluate_safety_zone()` function returning zone classification + effective clearance.
- **Jerk-Limited Emergency Stop**: `generate_jerk_limited_stop_profile()` produces S-curve deceleration profiles (ramp-up/hold/ramp-down jerk phases) for smoother stops.
- **Complementary Filter Cutoff Frequency**: Optional `cutoff_frequency_hz` parameter auto-derives alpha from `1 / (1 + 2*pi*fc*dt)`, replacing manual alpha tuning.
- **AGV Safety Demo**: `examples/agv_safety_demo.cpp` demonstrating the full pipeline -- sensor fusion, PID control, multi-zone envelope, state transitions, and emergency stop.
- **Production Improvements Tests**: 15 new test cases covering all above features.

## [0.1.0] - 2026-03-13

### Added
- **ROS 2 Ament Integration**: Optional `SAFETY_CORE_ENABLE_AMENT` flag for building as a colcon/ament_cmake package with `package.xml`.
- **CPack Packaging**: `safety_core_app` executable, TGZ/ZIP archive generation, CMake config export for downstream `find_package` consumption.

### Changed
- Install target includes `safety_core_app` binary.

## [0.0.9] - 2026-02-24

### Added
- **Safety Case Guard CI Job**: Verifies presence of all required safety-case artifacts in CI.
- **ISO Standards Crosswalk**: ISO 26262 / ISO 13849 evidence domain crosswalk document.
- **Functional Safety Plan**: Activity, role, and release-gate plan for safety lifecycle.
- **V&V Strategy**: Verification and validation strategy baseline.
- **Tool Confidence Register**: Confidence controls for safety evidence toolchain.
- **Change Impact Assessment Template**: Template for evaluating safety impact of changes.
- **Release Safety Evidence Checklist**: Gating checklist for release safety completeness.

### Changed
- Documentation reorganized under `markdown/` directory.

## [0.0.8] - 2026-02-23

### Added
- **CONTRIBUTING.md**: Maintainer workflow, PR checklist, and safety-case update expectations.
- **Test Utilities**: Consolidated `test_support.hpp` for shared test infrastructure.
- **Coverage Gate**: CI variables `COVERAGE_MIN_LINE` (80%) and `COVERAGE_MIN_BRANCH` (55%) for coverage threshold enforcement.
- **Safety Supervisor Tests**: Localization/drop monitors, setter wiring, safe-stop forcing.
- **Numeric Epsilon Constant**: Shared epsilon for floating-point comparisons.
- **Context Factory Tests**: Null supervisor wiring coverage.

## [0.0.7] - 2026-02-22

### Added
- **Safety Supervisor**: Deterministic warning/degraded/critical escalation to SafeStop with pluggable monitors (localization age, diagnostic drop rate).
- **Policy Guard**: CI job checking for banned APIs (exceptions, dynamic allocation, `abort`) in core safety code.
- **Trajectory Validator**: Obstacle envelope verification in motion trajectories.
- **Safety Case Artifacts**: Assumptions register, residual risk register, integration safety manual, verification protocol template.

## [0.0.6] - 2026-02-16

### Added
- **Fault Injection Tests**: Backward-clock, NaN-burst, and transport-drop robustness checks.
- **Pre-Commit Hook**: clang-format auto-fix + clang-tidy on staged files, `SKIP_CLANG_TIDY` option.
- **Coverage Build Profile**: `cmake --preset coverage` with gcov instrumentation.
- **CMake Package Config**: Exported targets for downstream `find_package(safety_core CONFIG)`.
- **No-Allocation Guards**: CI symbol-level checks extended to trajectory, filter, executor, and supervisor objects.
- **Expanded Test Suite**: PID, watchdog, factory, filter, trajectory, and diagnostic truncation tests.

### Changed
- Traceability matrix revised with requirement IDs and CI gate mapping.

## [0.0.5] - 2026-02-15

### Changed
- README expanded with full scope documentation covering control/filter components, health monitoring, and system integration examples.

## [0.0.4] - 2026-02-13

### Added
- **Platform Clock Abstraction**: Pluggable `Clock` interface with `ManualClock` for deterministic testing.
- **System Configuration**: `SystemConfig` with environment-variable overrides, schema migration, and validation (strict/allow-warnings policies).
- **Task Executor**: Bounded executor with configurable timing, watchdog windows, deadline-miss detection, and catch-up policies.
- **Context Factory**: `system::build_context()` one-call startup wiring.
- **Docking Mode**: New state machine mode with transition rules.

### Changed
- State machine modes renamed from `kPascalCase` to `PascalCase` (e.g. `kStandby` -> `Idle`, `kActive` -> `Moving`, `kObstacleHold` -> `AvoidingObstacle`).
- Sanitizer link options changed to PUBLIC for proper propagation to dependents.

### Fixed
- Line length violation in state machine transition validation for Idle mode.
- Comment alignment in `state_machine.hpp`.

## [0.0.3] - 2026-02-12

### Added
- **CI Pipeline**: Format check, clang-tidy, build/test stages.
- **GitLab SAST**: Static Application Security Testing template.
- **Secret Detection**: GitLab secret detection template.
- `.clang-format` and `.clang-tidy` configuration.

### Changed
- CI base image updated to Ubuntu 24.04.
- All headers reformatted with consistent code style.

## [0.0.2] - 2026-02-12

### Added
- Initial README with project scope and build instructions.

## [0.0.1] - 2026-02-12

### Added
- **State Machine**: Deterministic mode management with Init, Standby, Active, Degraded, AvoidingObstacle, LocalizationLost, SafeStop modes.
- **Safety Envelope**: Config-driven motion envelope enforcement with stop-distance calculations.
- **Diagnostics**: Health state with latched faults, pluggable health monitor callbacks.
- **Result Type**: Lightweight status return type for safety APIs.
- **Time Utilities**: Budget helpers and time-point abstractions.
- CMakeLists.txt with library and test targets.

[Unreleased]: https://gitlab.com/fredotran/safety-autonomy-core/-/compare/v0.1.0...HEAD
[0.1.0]: https://gitlab.com/fredotran/safety-autonomy-core/-/compare/v0.0.9...v0.1.0
[0.0.9]: https://gitlab.com/fredotran/safety-autonomy-core/-/compare/v0.0.8...v0.0.9
[0.0.8]: https://gitlab.com/fredotran/safety-autonomy-core/-/compare/v0.0.7...v0.0.8
[0.0.7]: https://gitlab.com/fredotran/safety-autonomy-core/-/compare/v0.0.6...v0.0.7
[0.0.6]: https://gitlab.com/fredotran/safety-autonomy-core/-/compare/v0.0.5...v0.0.6
[0.0.5]: https://gitlab.com/fredotran/safety-autonomy-core/-/compare/v0.0.4...v0.0.5
[0.0.4]: https://gitlab.com/fredotran/safety-autonomy-core/-/compare/v0.0.3...v0.0.4
[0.0.3]: https://gitlab.com/fredotran/safety-autonomy-core/-/compare/v0.0.2...v0.0.3
[0.0.2]: https://gitlab.com/fredotran/safety-autonomy-core/-/compare/v0.0.1...v0.0.2
[0.0.1]: https://gitlab.com/fredotran/safety-autonomy-core/-/tree/v0.0.1
