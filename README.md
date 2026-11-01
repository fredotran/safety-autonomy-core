# Safety Autonomy Core

[![CI](https://github.com/fredotran/safety-autonomy-core/actions/workflows/ci.yml/badge.svg)](https://github.com/fredotran/safety-autonomy-core/actions/workflows/ci.yml)
[![Release](https://github.com/fredotran/safety-autonomy-core/actions/workflows/release.yml/badge.svg)](https://github.com/fredotran/safety-autonomy-core/actions/workflows/release.yml)

**A high-assurance C++20 library for safety-critical robotics and autonomous vehicles.**

Designed for AGV/AMR platforms operating in dynamic industrial environments with pedestrians, forklifts, and unexpected obstacles.

---

> **Positioning:** safety-autonomy-core is a *defense-in-depth* layer with
> safety-grade engineering discipline — **not a certified safety channel**.
> ISO 3691-4 / ISO 13849 deployments still require safety-rated hardware
> (certified laser scanners, safety PLC, STO on the drives) as the authority.
> This library sits between Nav2-style convenience monitoring and that
> certified channel: deterministic, allocation-free, evidence-backed. See
> [How this compares](#how-this-compares) and the
> [safety case](markdown/docs/safety_case/) for certification-readiness status.

## Table of Contents

- [Quick Start](#quick-start)
- [Features](#features)
- [How this compares](#how-this-compares)
- [Demo](#demo)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Architecture](#architecture)
- [Documentation](#documentation)

---

## Quick Start

### Option 1: Docker Demo (Recommended)

The fastest way to see the safety system in action - no ROS2 installation required:

```bash
./setup_demo.sh
```

This launches a complete ROS2 simulation with Gazebo in an isolated Docker container.

### Option 2: Library Development

For C++ library development:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Features

### Core Safety Components

| Component | Description |
|-----------|-------------|
| **State Machine** | Deterministic mode management with thread-safe transitions |
| **Safety Envelope** | Multi-zone protection (Clear → Warning → Protective → Emergency) |
| **Control System** | Safety PID with anti-windup and feedforward support |
| **Sensor Fusion** | Bounded EKF, complementary filter, alpha-beta filter with timestamped updates |
| **Task Executor** | Allocation-free scheduling with watchdog and deadline detection |
| **Diagnostics** | Fixed-capacity events with health monitoring and pluggable transports |
| **Platform HAL** | Clock abstraction, IMU interface, odometry, and actuator control |
| **ROS Bridge Adapters** | `RosImuBridge`, `RosOdometryBridge`, `RosDriveActuatorBridge` for ROS2 integration |

### Advanced Capabilities

- **Visual Odometry**: ORB feature tracking for camera-based pose estimation
- **Adaptive EKF**: Dynamic process noise adjustment for wheel slip detection
- **IMU Bias Estimation**: Online gyro/accel bias calibration
- **Sensor Fault Detection**: Comprehensive monitoring for all sensors
- **Sensor Health Monitoring**: Real-time health tracking with graceful degradation
- **GPS Covariance Adaptation**: Quality-based covariance scaling for adaptive EKF fusion
- **Dynamic Safety Buffer**: Velocity-dependent safety margins for adaptive protection
- **Kidnapping Detection**: Pose jump analysis for unexpected repositioning
- **Localization Confidence**: Real-time uncertainty assessment

---

## How this compares

Where safety-autonomy-core sits relative to other open projects in the ROS 2 /
AGV-AMR safety space. Nothing below is a direct substitute — several entries are
complementary building blocks, or the certified hardware channel this library is
designed to sit alongside.

| Project | What it provides | How it relates |
|---------|------------------|----------------|
| [`nav2_collision_monitor`](https://github.com/ros-navigation/navigation2) | Polygon-based stop/slowdown/limit/approach zones on `LaserScan`/`PointCloud`, inside Nav2 | Closest functional overlap with `safety_envelope_node`. Nav2's maintainers state it is not a certified safety system: no mode state machine, no fault latching, dynamic allocation throughout, no safety-case scaffolding. We add those; it offers richer zone geometry (arbitrary polygons, multiple action models) |
| [`ros-safety/software_watchdogs`](https://github.com/ros-safety/software_watchdogs) | DDS-QoS/lifecycle watchdog + heartbeat nodes (ROS 2 Safety-Critical WG) | Complementary building blocks for liveliness monitoring. Our supervisor covers watchdog roles plus escalation, zone evaluation, and mode management in one deterministic pipeline |
| [`ros-safety/contracts_lite`](https://github.com/ros-safety/contracts_lite) | C++ contract checks (pre/post-conditions, invariants) | Orthogonal tooling; can complement this library's `[[nodiscard]]`/`Result` discipline |
| [Autoware MRM stack](https://github.com/autowarefoundation/autoware_universe) (`mrm_handler`, `system_error_monitor`, `emergency_handler`) | Hazard aggregation + minimum-risk-maneuver state machine for autonomous driving | Same supervisor→degraded→stop architecture at vehicle scale, but requires the Autoware ecosystem and targets road vehicles. Ours is minimal, ROS-agnostic at the core, and AGV-focused |
| [`autoware_sentinel`](https://github.com/NEWSLabNTU/autoware_sentinel) | Rust `no_std` safety island on Zephyr MCU for Autoware | Complementary architecture — an independent compute island is what real certification paths look like. Our ROS-free core library could run on such a partition |
| [`polymathrobotics/protective-stop`](https://github.com/polymathrobotics/protective-stop) | Open-hardware deadman / protective-stop remote (ESP32) with a public SIL 3 / PL e safety case | Complementary: an operator intent channel, not a motion safety layer |
| [OpenAMR safety package](https://github.com/openAMRobot/openamr-platform-sw) | Open AMR platform with concept-phase safety case / FMEA / requirements docs | Platform-specific engineering record rather than a reusable library; comparable documentation rigor |
| [`sick_safetyscanners2`](https://github.com/SICKAG/sick_safetyscanners2) · [`psen_scan_v2`](https://github.com/PilzDE/psen_scan_v2) | ROS 2 drivers for certified safety laser scanners (PL d / SIL 2, IEC 61496) | Complementary — the certified sensor channel. Our envelope can consume their scans while an independent safety PLC remains the authority |
| [EasyNav](https://github.com/EasyNavigation/EasyNavigation) | Nav2-alternative stack with an explicit "we are not the safety channel" boundary | Philosophically adjacent: it delegates all safety functions to certified hardware and provides only reflex braking; we provide the disciplined defense-in-depth layer in between |
| [`SOTERonROS`](https://github.com/Drona-Org/SOTERonROS) | Academic runtime-assurance (Simplex) framework | Research-grade, ROS 1, dormant |
| eProsima [Safe DDS](https://www.eprosima.com/middleware/safe-dds) | ISO 26262 ASIL-D certified DDS middleware | Proprietary, not open source — relevant if a deployment needs certified comms; interoperates with ROS 2 |
| Single-purpose e-stop/teleop nodes ([`twist_mux`](https://github.com/ros-teleop/twist_mux), ad-hoc estop multiplexers, teleop safety nodes) | Command arbitration, deadman timeouts, velocity clamps | Single-purpose; `safety_drive_bridge_node` covers their role plus command-freshness watchdog and jerk-limited stop profiles |

**In short:** no open-source project today combines this scope in one package.
The differentiation is engineering discipline — allocation-free `noexcept` core,
enforced static-analysis gate, coverage/fuzz/timing gates — plus
safety-case-as-code under `markdown/docs/safety_case/`, not novel algorithms.

## Demo

### Interactive Demo System

The project includes a comprehensive ROS2 demo with Gazebo simulation:

**Demo Modes:**
- **Quick Demo** (1-2 min): Fast validation of core features
- **Comprehensive Demo** (5-7 min): Complete demonstration with edge cases
- **Interactive Demo**: Menu-driven scenario testing
- **Sensor Failure Simulation**: Realistic fault testing

**Features:**
- Industrial warehouse simulation with moving obstacles
- Real-time safety zone visualization
- Colored console output for easy monitoring
- Interactive menu system

### Quick Demo Start

```bash
./setup_demo.sh
```

### Docker vs Local

| Aspect | Docker | Local |
|--------|--------|-------|
| **Setup Time** | ~2 minutes | ~30 minutes |
| **ROS2 Required** | No | Yes |
| **Consistency** | Guaranteed | Variable |
| **GPU Support** | Easy | Manual |
| **Isolation** | Yes | No |

---

## Requirements

### For Library Development

- **CMake** 3.20+
- **C++20** compiler (GCC 12+ or Clang 15+)
- **Eigen3** for matrix operations
- **Optional**: ROS 2 Humble/Iron (for ament integration)

### For Docker Demo

- **Docker Engine** 20.10+
- **Docker Compose** 2.0+
- **Optional**: NVIDIA Docker runtime (GPU support)
- **Optional**: X11 server (display forwarding)

---

## Installation

### Method 1: Docker (Recommended)

```bash
# Clone the repository
git clone <repository-url>
cd safety-autonomy-core

# Run the demo
./setup_demo.sh
```

### Method 2: Build from Source

```bash
# Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DSAFETY_CORE_ENABLE_SANITIZERS=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# Install (optional)
cmake --install build
```

### Build Presets

```bash
cmake --preset dev      # Development with sanitizers
cmake --preset safety   # Safety-critical release build
cmake --preset coverage # Coverage instrumentation
```

---

## Usage

### Library Integration

```cmake
find_package(safety_core CONFIG)
target_link_libraries(my_app PRIVATE safety_core::safety_core)
```

### ROS 2 Integration

```bash
# Build as ROS2 package
colcon build --packages-select safety_autonomy_core \
    --cmake-args -DSAFETY_CORE_ENABLE_AMENT=ON

# Launch demo
ros2 launch safety_core_bringup agv_warehouse.launch.py
```

### Configuration

The library supports environment-based configuration:

```cpp
auto context = system::build_context();
// Apply environment overrides automatically
// Migrate legacy schemas
// Validate configuration
```

---

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────┐
│                   Application Layer                │
├─────────────────────────────────────────────────────┤
│              Safety Autonomy Core Library          │
├─────────────────────────────────────────────────────┤
│  State Machine │ Safety Envelope │ Filters │ PID   │
├─────────────────────────────────────────────────────┤
│              Platform HAL (Clock, IMU, ...)        │
├─────────────────────────────────────────────────────┤
│              ROS 2 Integration (Optional)           │
└─────────────────────────────────────────────────────┘
```

### Safety Supervisor

Deterministic escalation pipeline:
```
Normal → Warning → Degraded → SafeStop (Critical)
```

Monitors:
- Localization age
- Diagnostic drop rate
- Obstacle proximity
- Each monitor independently triggers escalation

The ROS 2 layer adds stream-level watchdogs (scan/odom/envelope staleness) on
top — a dead sensor pipeline latches fault `0xE004` and forces SafeStop. See
[`ros2/README.md`](ros2/README.md).

### Multi-Zone Safety Envelope

```
┌─────────────────────────────────────┐
│           CLEAR ZONE                │  Full speed
├─────────────────────────────────────┤
│         WARNING ZONE                │  Reduced speed
├─────────────────────────────────────┤
│       PROTECTIVE ZONE               │  Controlled decel
├─────────────────────────────────────┤
│       EMERGENCY ZONE                │  Immediate stop
└─────────────────────────────────────┘
```

---

## Documentation

### Core Documentation

- **[OVERVIEW.md](OVERVIEW.md)** - Comprehensive project overview including testing and quality assurance
- **[CHANGELOG.md](CHANGELOG.md)** - Version history and changes
- **[CONTRIBUTING.md](CONTRIBUTING.md)** - Contribution guidelines
- **[CI_DOCUMENTATION.md](CI_DOCUMENTATION.md)** - CI/CD pipeline documentation

### Technical Documentation

- **[markdown/docs/DOCKER.md](markdown/docs/DOCKER.md)** - Docker deployment guide
- **[markdown/docs/safety_case/](markdown/docs/safety_case/)** - ISO 26262/ISO 13849 evidence
- **[ros2/README.md](ros2/README.md)** - ROS 2 integration details
- **[ros2/LOCALIZATION.md](ros2/LOCALIZATION.md)** - Advanced localization stack

### Demo Documentation

- **[DEMO_GUIDE.md](DEMO_GUIDE.md)** - Complete demo guide
- **[DEMO_TROUBLESHOOTING.md](DEMO_TROUBLESHOOTING.md)** - Troubleshooting guide

---

## Safety-Critical Features

### Compliance

- **MISRA C++**: MISRA/AUTOSAR-inspired coding rules
- **ISO 26262**: Functional safety compliance scaffolding
- **ISO 13849**: Safety-related parts of control systems
- **No Dynamic Allocation**: Deterministic memory usage in real-time paths

### Safety Guarantees

- **Thread-Safe**: Atomic operations for concurrent access
- **Bounded Operations**: Fixed-capacity containers and queues
- **Deterministic Timing**: Bounded execution time guarantees
- **Comprehensive Error Handling**: Graceful degradation and recovery

### Testing & Evidence

**Library:** 19 test executables covering state machine transitions, PID
boundaries/anti-windup, executor watchdog and deterministic replay, filter
invariants (bounded EKF, complementary, alpha-beta), motion/jerk limits,
envelope zones, supervisor escalation, env-config overrides, fault injection
(backward clock, NaN bursts, transport drops), no-allocation symbol guards,
deterministic property tests, and coverage-gap regressions.

**ROS 2 layer** (`ros2/src/*/test`, run by the `ros2_tests` CI job via
`colcon test`): `test_safety_scenario` — 9 scenario tests over the end-to-end
fail-safe chain (clear drive, protective hold, pop-up obstacle → latched
SafeStop, stale scan stamp → EMERGENCY, scan-stream loss → SafeStop,
envelope-node loss → fault `0xE004`, stale/missing odometry chains) — plus
bridge/param/QoS utilities, envelope-node suite, `IsSafe` BT condition,
message interfaces, and launch/sim-asset validation.

Additional verification targets:

```bash
# libFuzzer harnesses (requires Clang)
cmake -S . -B build-fuzz -DSAFETY_CORE_ENABLE_FUZZING=ON
cmake --build build-fuzz --target fuzz_env_loader fuzz_filters
./build-fuzz/fuzz_filters -max_total_time=60

# Timing-evidence benchmarks (also registered as a ctest)
cmake --build build --target safety_core_benchmarks
./build/safety_core_benchmarks --iterations 50000 --json
```

---

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SAFETY_CORE_ENABLE_SANITIZERS` | `ON` | Address/UB sanitizers for dev builds |
| `SAFETY_CORE_ENABLE_WERROR` | `ON` | Treat warnings as errors |
| `SAFETY_CORE_ENABLE_COVERAGE` | `OFF` | GCC coverage instrumentation |
| `SAFETY_CORE_ENABLE_SIZE_OPTIMIZATIONS` | `OFF` | Size optimizations for low-resource systems (NOT for safety-critical use) |
| `SAFETY_CORE_ENABLE_FUZZING` | `OFF` | libFuzzer harnesses (requires Clang) |
| `SAFETY_CORE_ENABLE_AMENT` | `OFF` | ROS 2 ament_cmake integration |

Additional build targets: `safety_core_certifiable` (same sources under
`-fno-exceptions -fno-rtti`, see
`markdown/docs/safety_case/certifiable_partition.md`) and
`safety_core_benchmarks` (timing evidence, see
`markdown/docs/safety_case/timing_evidence.md`).

### Build Presets

| Preset | Use Case | Safety-Critical |
|---------|----------|-----------------|
| `dev` | Development with sanitizers | No |
| `safety` | Safety-critical release build | **Yes** |
| `coverage` | Coverage instrumentation | No |
| `minimal` | Size-optimized for low-resource systems | **No** |

**IMPORTANT**: For safety-critical applications, always use the `safety` preset (Release build). The `minimal` preset prioritizes size over performance and may affect timing determinism, making it unsuitable for safety-critical systems.

---

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Workflow

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests and quality gate
5. Submit a pull request

### Code Quality

- Follow MISRA/AUTOSAR-inspired coding rules
- Use `clang-format` for code formatting
- `clang_tidy` CI job runs cppcheck (`--enable=all`) + clang-tidy, fully
  blocking; `tools/ci/static_analysis_gate.sh` provides an optional local
  baseline ratchet (`--update` to shrink tracked debt)
- Coverage gate: line ≥ 80%, branch ≥ 70%
- Additional CI gates: `fuzz_smoke`, `benchmarks` (timing evidence),
  `certifiable_build` (no-exceptions/RTTI partition), `sbom`,
  `ros2_tests` (`colcon test` in the built image), hazard→test traceability
  (`tools/safety/verify_traceability.py`)
- Ensure all tests pass

---

## Packaging

Generate installable archives:

```bash
cmake --build build --target package
```

Produces `.tar.gz` and `.zip` artifacts with CMake config files for downstream integration.

---

## License

MIT License - See [LICENSE](LICENSE) file for details

---

## Support

- **Documentation**: See [OVERVIEW.md](OVERVIEW.md) for comprehensive project information
- **CI/CD**: See [CI_DOCUMENTATION.md](CI_DOCUMENTATION.md) for pipeline details
- **Issues**: Open GitHub issues for bugs or feature requests
- **Discussions**: Use GitHub Discussions for questions and ideas

---

**For detailed project architecture, safety-critical considerations, and future roadmap, see [OVERVIEW.md](OVERVIEW.md)**
