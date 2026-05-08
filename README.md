# Safety Autonomy Core

[![CI](https://github.com/fredotran/safety-autonomy-core/actions/workflows/ci.yml/badge.svg)](https://github.com/fredotran/safety-autonomy-core/actions/workflows/ci.yml)
[![Release](https://github.com/fredotran/safety-autonomy-core/actions/workflows/release.yml/badge.svg)](https://github.com/fredotran/safety-autonomy-core/actions/workflows/release.yml)

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

### For Library Development
- CMake 3.20+
- C++20 compiler (GCC 12+ or Clang 15+)
- Optional: ROS 2 Humble/Iron (for ament integration)

### For Docker Demo (Recommended)
- Docker Engine 20.10+
- Docker Compose 2.0+
- For GPU support: NVIDIA Docker runtime (nvidia-docker2)
- For display forwarding: X11 server (Linux/Mac) or XQuartz (Mac)

## Quick Start

### Docker Demo (Recommended - No ROS2 Installation Required)

The fastest way to see the safety system in action:

```bash
# Run the interactive demo launcher
./setup_demo.sh
# Select demo option from the menu
```

This uses Docker to run the complete ROS2 Jazzy + Gazebo simulation without requiring any local ROS2 installation. See [DOCKER.md](DOCKER.md) for complete Docker setup guide.

### Library Development

For library development and testing:

```bash
# Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSAFETY_CORE_ENABLE_SANITIZERS=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# Run AGV safety demo (requires local ROS2)
./build/agv_safety_demo
```

## Demo System

The project includes a comprehensive ROS 2 demo system with Gazebo simulation for testing and demonstration:

### Quick Demo Start

**🐳 Docker (Recommended - No ROS2 Installation Required)**

Docker is the default and recommended way to run the demo. It provides a complete, isolated environment with ROS2 Jazzy, Gazebo Harmonic, Nav2, and all dependencies pre-configured.

```bash
./setup_demo.sh
# Select demo option from the interactive menu
```

**Docker Benefits:**
- ✅ No local ROS2 installation required
- ✅ Consistent environment across all platforms
- ✅ GPU support for Gazebo rendering
- ✅ Display forwarding for RViz visualization
- ✅ Easy setup with single command
- ✅ Isolated from system dependencies

**Docker Requirements:**
- Docker Engine 20.10+
- Docker Compose 2.0+
- NVIDIA Docker runtime (for GPU support)
- X11 server (for display forwarding)

See [DOCKER.md](DOCKER.md) for complete Docker deployment guide, including:
- GPU setup and configuration
- Display forwarding for Linux/Mac/Windows
- Hardware integration (sensors, actuators)
- Performance optimization
- Troubleshooting common issues

**💻 Local ROS2 Installation**

For developers with local ROS2 Jazzy installation:

```bash
./setup_demo.sh --local
# Select option 1 (Full demo) or option 2 (Safety stack only)
```

**Local Requirements:**
- ROS 2 Jazzy installed
- Gazebo Harmonic
- Nav2 navigation stack
- All ROS2 dependencies installed via rosdep

### Demo Modes

- **Quick Demo** (1-2 min): Fast validation of core safety features
- **Comprehensive Demo** (5-7 min): Complete demonstration with all edge cases
- **Interactive Demo**: Menu-driven on-demand scenario testing
- **Performance Benchmark**: System performance measurement and analysis
- **Before/After Comparison**: Safety system value demonstration
- **Sensor Failure Simulation**: Realistic sensor failure testing

### Demo Features

- **Colored console output** for better visual feedback
- **Real-time metrics display** showing zone transitions and mode changes
- **Moving obstacles** (conveyor belt, forklift) for dynamic scenarios
- **Realistic sensor failures** (noise, dropout, latency simulation)
- **Interactive menu system** for on-demand testing

### Documentation

- [DOCKER.md](DOCKER.md) - Complete Docker deployment guide, including GPU support, display forwarding, and hardware integration
- [DEMO_GUIDE.md](DEMO_GUIDE.md) - Complete demo documentation, including detailed instructions for each demo mode, best practices, and advanced usage examples
- [DEMO_TROUBLESHOOTING.md](DEMO_TROUBLESHOOTING.md) - Comprehensive troubleshooting guide for simulation, safety nodes, demo scripts, and performance issues
- [ROS 2 README](ros2/README.md) - ROS 2 integration details and package documentation
- [RVIZ_FIX_GUIDE.md](ros2/RVIZ_FIX_GUIDE.md) - Quick fix guide for RViz display issues
- [WHEEL_ODOMETRY_NAVIGATION.md](ros2/WHEEL_ODOMETRY_NAVIGATION.md) - Configuration guide for wheel odometry-based navigation

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
│   └── docs/                  #   Documentation
│       ├── diagnostics/       #   Integration guides
│       └── safety_case/       #   ISO 26262 / ISO 13849 evidence scaffolding
├── ros2/                      # ROS 2 wrapper + AGV simulation
│   └── src/                   #   ROS 2 packages (safety_core_msgs, safety_core_ros, etc.)
├── Dockerfile                 # Multi-stage Docker build for ROS2 demo
├── docker-compose.yml         # Docker orchestration with GPU/display support
├── .dockerignore              # Docker build context optimization
├── .env.example               # Environment variable template for Docker
├── Makefile                   # Docker convenience commands
├── DOCKER.md                  # Complete Docker deployment guide
├── setup_demo.sh              # ROS 2 demo build + launch script (Docker-first)
├── README.md                  # Project documentation
├── CHANGELOG.md               # Version history
├── CONTRIBUTING.md            # Contribution guidelines
├── LICENSE                    # Commercial license terms
├── CMakeLists.txt             # Build system
├── CMakePresets.json          # Dev/safety/coverage presets
├── .clang-format              # Code style
├── .clang-tidy                # Static analysis rules
├── .github/workflows/         # GitHub Actions CI + release-please pipeline
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

**🐳 Quick Start with Docker (Recommended):**
```bash
./setup_demo.sh
# Select comprehensive demo from the menu
```

**💻 Manual Local Setup:**
```bash
cd ros2
source /opt/ros/jazzy/setup.bash
colcon build --packages-skip safety_core_nav2 \
    --cmake-args -DSAFETY_CORE_ENABLE_AMENT=ON \
                 -DSAFETY_CORE_ENABLE_SANITIZERS=OFF \
                 -DSAFETY_CORE_ENABLE_WERROR=OFF
source install/setup.bash
ros2 launch safety_core_bringup agv_warehouse.launch.py
```

**Comprehensive demo (showcases all safety capabilities):**
```bash
ros2 launch safety_core_bringup comprehensive_demo.launch.py
```

See [`ros2/README.md`](ros2/README.md) for full architecture, topic map, parameter list, and run instructions.

For a comprehensive demonstration of all safety capabilities (state machine transitions, safety zones, fault handling, emergency stops), see the [comprehensive demo documentation](ros2/COMPREHENSIVE_DEMO.md).

### Advanced Localization Stack

The ROS 2 integration includes a comprehensive localization stack with advanced sensor fusion, fault detection, and environment-specific configurations:

**Multi-Sensor Fusion:**
- **Visual Odometry Node**: ORB feature tracking using OpenCV for camera-based pose estimation
- **Adaptive EKF**: Dynamic process noise adjustment based on wheel slip detection
- **IMU Bias Estimation**: 21-state EKF with online gyro/accel bias calibration
- **Sensor Fault Detection**: Comprehensive fault monitoring for wheel odometry, IMU, GPS, and visual odometry

**Enhanced SLAM:**
- **Warehouse-Optimized SLAM**: Extended loop closure parameters for structured environments
- **Localization Mode**: Switch from mapping to localization using pre-built maps
- **Environment-Specific Presets**: Optimized parameters for outdoor (GPS), indoor (GPS-denied), and warehouse (map-based) scenarios

**Robustness Features:**
- **Kidnapping Detection**: Pose jump analysis for detecting unexpected robot repositioning
- **Localization Confidence Scoring**: Real-time uncertainty assessment
- **TF Tree Consistency Monitoring**: Detect and report coordinate system issues
- **Sensor Timeout Monitoring**: Detect and handle sensor unavailability

**Configuration Files:**
- `config/ekf_config_with_bias.yaml` - EKF with IMU bias estimation
- `config/slam_toolbox_warehouse.yaml` - Warehouse-optimized SLAM parameters
- `config/slam_toolbox_localization.yaml` - Localization mode configuration
- `config/params_outdoor.yaml` - Outdoor navigation with GPS
- `config/params_indoor.yaml` - Indoor GPS-denied SLAM
- `config/params_warehouse.yaml` - Warehouse map-based navigation

**New Scripts:**
- `scripts/visual_odometry_node.py` - Visual odometry with ORB features
- `scripts/adaptive_ekf_node.py` - Adaptive EKF with wheel slip detection
- `scripts/sensor_fault_detector.py` - Comprehensive sensor fault monitoring
- `scripts/enhanced_localization_monitor.py` - Kidnapping detection and confidence scoring

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

GitHub Actions workflows live under [`.github/workflows/`](.github/workflows/):

| Stage | Job | Description |
|-------|-----|-------------|
| Lint | `format_check` | clang-format guard |
| Lint | `clang_tidy` | Static analysis (currently `continue-on-error` while pre-existing findings are cleaned up) |
| Lint | `hook_smoke` | Pre-commit hook validation |
| Policy | `policy_guard` | Banned API checks (exceptions, dynamic alloc, abort) |
| Policy | `safety_case_guard` | Safety artifact presence verification |
| Build | `build_and_test` | CMake build + ctest (sanitizers enabled) |
| Coverage | `coverage` | gcovr gate (line: 80%, branch: 55%) |
| ROS 2 | `docker_build` | Docker Compose build of ROS 2 packages (Jazzy + Gazebo + Nav2) |

## Release Process

Releases are automated via **[release-please](https://github.com/googleapis/release-please-action)**. The workflow lives in [`.github/workflows/release.yml`](.github/workflows/release.yml).

Flow:
1. Land [Conventional Commits](https://www.conventionalcommits.org/) on `main` (`feat:`, `fix:`, `feat!:` for breaking changes, etc.).
2. release-please opens / updates a release PR on `main` that bumps the version (CMake `project(... VERSION ...)` + every `package.xml`) and updates `CHANGELOG.md`.
3. Merging the release PR creates an annotated git tag (e.g. `v0.2.0`) **and** a GitHub Release with the changelog notes.

Config: [`.release-please-config.json`](.release-please-config.json) and [`.release-please-manifest.json`](.release-please-manifest.json).

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

MIT License - see [LICENSE](LICENSE) file for details.

Copyright (c) 2026 RedEarth OS
