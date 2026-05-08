# Safety Autonomy Core

High-assurance C++20 library for safety-critical robotics and autonomous vehicles. Provides deterministic state machines, bounded executors, sensor fusion filters, motion primitives, and multi-zone safety envelopes with MISRA/AUTOSAR-inspired coding rules.

Designed for AGV/AMR platforms operating in dynamic warehouses (pedestrians, forklifts, pop-up obstacles, temporary localization loss).

## Features

| Module | Description |
|--------|-------------|
| **State Machine** | Deterministic mode management (Init, Idle, Moving, Degraded, AvoidingObstacle, LocalizationLost, Docking, SafeStop) with thread-safe atomics and latched faults |
| **Control** | Safety PID with anti-windup (integral clamping + back-calculation), feedforward, localization-stale guard |
| **Filters** | Bounded EKF (innovation gating, finite guards), complementary filter (auto cutoff-freq derivation), alpha-beta filter -- all with timestamped update APIs |
| **Safety** | Multi-zone envelope (Clear/Warning/Protective/Emergency), robot footprint geometry, deterministic supervisor with escalation to SafeStop |
| **Motion** | Trajectory primitives, jerk-limited emergency stop profile, speed/accel/jerk validator |
| **Executor** | Bounded task executor with allocation-free callbacks, configurable watchdog, deadline-miss detection, catch-up policies |
| **Diagnostics** | Fixed-capacity diagnostic events (no heap in transport path), health beacons, truncation observability, pluggable transports |
| **Platform HAL** | Clock abstraction, 6-DOF IMU interface, odometry increments, drive actuator with command freshness watchdog |

## Requirements

- CMake 3.20+
- C++20 compiler (GCC 12+ or Clang 15+)
- Optional: ROS 2 Humble/Iron (for ament integration)

## Quick Start

```bash
# Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSAFETY_CORE_ENABLE_SANITIZERS=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# Run AGV safety demo
./build/agv_safety_demo
```

## Repository Layout

```
safety-autonomy-core/
├── include/safety_core/       # Public headers
│   ├── common/                #   Time utilities, result types
│   ├── config/                #   System config, env loader, migration, validation
│   ├── control/               #   Safety PID controller
│   ├── diag/                  #   Diagnostic transport, health beacons, topics
│   ├── diagnostics/           #   Health state definitions
│   ├── exec/                  #   Task executor
│   ├── filters/               #   EKF, complementary, alpha-beta filters
│   ├── motion/                #   Trajectory primitives
│   ├── platform/              #   Clock, sensors (IMU, odometry), actuators, watchdog
│   ├── safety/                #   Safety envelope, supervisor, zones
│   ├── state_machine/         #   Mode state machine
│   └── system/                #   Context factory, system context
├── src/                       # Implementations
├── tests/                     # Unit and property tests
├── examples/                  # Demo applications
│   └── agv_safety_demo.cpp    #   Full AGV safety pipeline demonstration
├── tools/                     # Developer tooling
│   └── dev/run_quality_gate.sh
├── markdown/                  # Documentation and safety case artifacts
│   ├── README.md              #   Project documentation (symlink at root)
│   ├── CHANGELOG.md           #   Version history (symlink at root)
│   ├── CONTRIBUTING.md        #   Contribution guidelines (symlink at root)
│   └── docs/safety_case/      #   ISO 26262 / ISO 13849 evidence scaffolding
├── ros2/                      # ROS 2 wrapper + AGV simulation
│   └── src/                   #   ROS 2 packages (safety_core_msgs, safety_core_ros, etc.)
├── setup_demo.sh              # ROS 2 demo build + launch script
├── LICENSE                    # Commercial license terms
├── CMakeLists.txt             # Build system
├── CMakePresets.json          # Dev/safety/coverage presets
├── .clang-format              # Code style
├── .clang-tidy                # Static analysis rules
├── .gitlab-ci.yml             # CI pipeline
└── package.xml                # ROS 2 ament package manifest
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SAFETY_CORE_ENABLE_SANITIZERS` | `ON` | Address/UB sanitizers for dev builds |
| `SAFETY_CORE_ENABLE_WERROR` | `ON` | Treat warnings as errors |
| `SAFETY_CORE_ENABLE_COVERAGE` | `OFF` | GCC coverage instrumentation |
| `SAFETY_CORE_ENABLE_AMENT` | `OFF` | ROS 2 ament_cmake integration |

## Build Presets

```bash
cmake --preset dev      # RelWithDebInfo + sanitizers + -Werror
cmake --preset safety   # Release + no sanitizers + -Werror
cmake --preset coverage # Debug + gcov instrumentation
```

Build and test with:
```bash
cmake --build --preset <name>
ctest --preset <name>
```

## Packaging

Generate installable archives (library + headers + demo app):

```bash
cmake --build build --target package
```

Produces `.tar.gz` and `.zip` artifacts (e.g. `safety-core-0.1.0-Linux-x86_64.tar.gz`). Includes exported CMake config files so downstream projects can:

```cmake
find_package(safety_core CONFIG)
target_link_libraries(my_target PRIVATE safety_core::safety_core)
```

## ROS 2 Integration

Build as an ament_cmake package:

```bash
colcon build --packages-select safety_autonomy_core --cmake-args -DSAFETY_CORE_ENABLE_AMENT=ON
```

Downstream ROS 2 packages link with:

```cmake
find_package(safety_autonomy_core REQUIRED)
target_link_libraries(my_node PRIVATE safety_autonomy_core::safety_core)
```

### ROS 2 wrapper + AGV warehouse simulation

A complete ROS 2 Jazzy + Gazebo Harmonic + Nav2 wrapper with a self-contained industrial warehouse simulation lives in [`ros2/`](ros2/README.md). It includes:

- Wrapper nodes (`safety_envelope_node`, `safety_supervisor_node`, `safety_drive_bridge_node`)
- Custom messages (`SafetyState`, `EnvelopeStatus`, `DiagnosticEvent`, etc.)
- A Nav2 BT plugin (`IsSafe` condition node)
- AGV URDF + warehouse SDF + `ros_gz_bridge` config
- Top-level launch files, Nav2 params, SLAM Toolbox config, RViz visualization

```bash
# Quick setup (build + launch)
./setup_demo.sh

# Or manually
cd ros2
source /opt/ros/jazzy/setup.bash
colcon build --packages-skip safety_core_nav2 \
    --cmake-args -DSAFETY_CORE_ENABLE_AMENT=ON \
                 -DSAFETY_CORE_ENABLE_SANITIZERS=OFF \
                 -DSAFETY_CORE_ENABLE_WERROR=OFF
source install/setup.bash
ros2 launch safety_core_bringup agv_warehouse.launch.py
```

See [`ros2/README.md`](ros2/README.md) for full architecture, topic map, parameter list, and run instructions.

## Architecture Overview

### Startup Flow

`system::build_context(...)` centralizes initialization:

1. Start from default `config::SystemConfig`
2. Apply environment overrides via `config::load_from_env`
3. Migrate legacy schema versions via `config::migrate_to_current`
4. Validate config (`Strict` or `AllowWarnings` policy)
5. Build a wired `SystemContext` (config, clock, monitor, transport, optional supervisor)

### Safety Supervisor

Deterministic escalation pipeline:

```
Normal → Warning → Degraded → SafeStop (Critical)
```

Monitors: localization age, diagnostic drop rate, obstacle proximity. Each monitor independently triggers escalation through the supervisor.

### Multi-Zone Safety Envelope

```
┌─────────────────────────────────────┐
│           CLEAR ZONE                │  Full speed
├─────────────────────────────────────┤
│         WARNING ZONE                │  Reduced speed limit
├─────────────────────────────────────┤
│       PROTECTIVE ZONE               │  Controlled deceleration
├─────────────────────────────────────┤
│       EMERGENCY ZONE                │  Immediate stop (jerk-limited)
└─────────────────────────────────────┘
```

Zone evaluation considers robot footprint geometry (length, width, front overhang) and returns effective clearance + recommended action.

### Executor Model

Allocation-free task scheduling:

```cpp
using TaskFn = Result (*)(time::TimePoint, void*) noexcept;
executor.add_task(task_fn, task_context, period);
executor.set_catch_up_policy(exec::CatchUpPolicy::BoundedCatchUp, 2U);
```

### Diagnostic Events

Fixed-capacity, zero-allocation event transport:

```
pid.localization_stale    pid.output
executor.task_added       executor.deadline_miss
safety.monitor_warning    safety.safestop_forced
health.beacon             config.migration
```

## Testing

19 test executables covering:

- State machine transitions and fault latching
- PID controller boundaries, anti-windup, feedforward
- Executor watchdog, deadline miss, deterministic replay
- Filter invariants (bounded EKF, complementary, alpha-beta)
- Innovation gating and timestamped updates
- Motion trajectory validation and jerk-limited stop profiles
- Safety envelope zones and robot footprint evaluation
- Safety supervisor escalation and monitor wiring
- Context factory env overrides and validation hooks
- Fault injection (backward clock, NaN bursts, transport drops)
- No-allocation policy verification (symbol-level guards)
- Diagnostic truncation observability

Run all tests:
```bash
ctest --test-dir build --output-on-failure
```

## Quality Gate

One-command local maintainer gate:

```bash
tools/dev/run_quality_gate.sh            # Build + tests + policy
tools/dev/run_quality_gate.sh --coverage  # Includes coverage gate
```

## Git Hooks

Enable repository-managed hooks:

```bash
git config core.hooksPath .githooks
```

Pre-commit runs:
- `clang-format` auto-fix on staged C/C++ files
- `clang-tidy` analysis (requires `build/compile_commands.json`)

Controls:
- `AUTO_FIX_FORMAT=0` -- check-only formatting
- `SKIP_CLANG_TIDY=1` -- skip tidy analysis

## CI Pipeline

| Stage | Job | Description |
|-------|-----|-------------|
| Lint | `format_check` | clang-format guard |
| Lint | `clang_tidy` | Static analysis |
| Lint | `hook_smoke` | Pre-commit hook validation |
| Policy | `policy_guard` | Banned API checks (exceptions, dynamic alloc, abort) |
| Policy | `safety_case_guard` | Safety artifact presence verification |
| Build | `build_and_test` | CMake build + ctest (sanitizers enabled) |
| Coverage | `coverage` | gcovr gate (line: 80%, branch: 55%) |
| Security | SAST + Secret Detection | GitLab templates |

## Coding Standards

MISRA/AUTOSAR-inspired rules for safety-critical code:

- No dynamic allocation in real-time paths
- No exceptions or RTTI in core modules
- Fixed-capacity containers throughout
- Explicit initialization, `[[nodiscard]]` on status APIs
- Saturation/NaN/Inf guards at all boundaries
- Deterministic math (no `-ffast-math`)
- Bounded queues, timed waits, no unbounded locks

## Safety Case Artifacts

Located in `markdown/docs/safety_case/`:

- Traceability matrix (hazard-to-control-to-test)
- Assumptions and residual risk registers
- ISO 3691-4 clause conformance matrix
- ISO 26262 / ISO 13849 crosswalk
- Functional safety plan
- V&V strategy
- Tool confidence register
- Change impact assessment template
- Release safety evidence checklist
- Integration safety manual

> **Note:** This repository provides process and evidence scaffolding for ISO 26262 / ISO 13849 readiness. Formal compliance requires project-specific system integration evidence, independent assessment, and organizational process records.

## Contributing

See `CONTRIBUTING.md` for:
- PR checklist and review process
- Safety-case update expectations
- Code style and testing requirements

## License

Proprietary -- per-robot/per-project commercial licensing with mandatory support/maintenance.
