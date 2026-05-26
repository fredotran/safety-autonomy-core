# Safety Autonomy Core

[![CI](https://github.com/fredotran/safety-autonomy-core/actions/workflows/ci.yml/badge.svg)](https://github.com/fredotran/safety-autonomy-core/actions/workflows/ci.yml)
[![Release](https://github.com/fredotran/safety-autonomy-core/actions/workflows/release.yml/badge.svg)](https://github.com/fredotran/safety-autonomy-core/actions/workflows/release.yml)

**A high-assurance C++20 library for safety-critical robotics and autonomous vehicles.**

Designed for AGV/AMR platforms operating in dynamic industrial environments with pedestrians, forklifts, and unexpected obstacles.

---

## Table of Contents

- [Quick Start](#quick-start)
- [Features](#features)
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

---

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SAFETY_CORE_ENABLE_SANITIZERS` | `ON` | Address/UB sanitizers for dev builds |
| `SAFETY_CORE_ENABLE_WERROR` | `ON` | Treat warnings as errors |
| `SAFETY_CORE_ENABLE_COVERAGE` | `OFF` | GCC coverage instrumentation |
| `SAFETY_CORE_ENABLE_SIZE_OPTIMIZATIONS` | `OFF` | Size optimizations for low-resource systems (NOT for safety-critical use) |
| `SAFETY_CORE_ENABLE_AMENT` | `OFF` | ROS 2 ament_cmake integration |

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
- Run `clang-tidy` for static analysis
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
