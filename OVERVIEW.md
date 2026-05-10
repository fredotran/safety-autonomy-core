# Safety Autonomy Core - Project Overview

## Project Summary

**Safety Autonomy Core** is a high-assurance C++20 library designed for safety-critical robotics and autonomous vehicles. It provides deterministic state machines, bounded executors, sensor fusion filters, motion primitives, and multi-zone safety envelopes with MISRA/AUTOSAR-inspired coding rules.

**Target Applications**: AGV/AMR platforms operating in dynamic warehouses with pedestrians, forklifts, pop-up obstacles, and temporary localization loss.

## Key Features

### Core Safety Components
- **State Machine**: Deterministic mode management (Init, Idle, Moving, Degraded, AvoidingObstacle, LocalizationLost, Docking, SafeStop) with thread-safe atomics
- **Control**: Safety PID with anti-windup, feedforward, and localization-stale guard
- **Filters**: Bounded EKF, complementary filter, alpha-beta filter with timestamped update APIs
- **Safety**: Multi-zone envelope (Clear/Warning/Protective/Emergency) with deterministic supervisor
- **Motion**: Trajectory primitives and jerk-limited emergency stop profiles
- **Executor**: Bounded task executor with allocation-free callbacks and watchdog
- **Diagnostics**: Fixed-capacity diagnostic events with health beacons
- **Platform HAL**: Clock abstraction, IMU interface, odometry, drive actuator

### Advanced Localization Stack
- **Visual Odometry**: ORB feature tracking using OpenCV
- **Adaptive EKF**: Dynamic process noise adjustment based on wheel slip detection
- **IMU Bias Estimation**: 21-state EKF with online gyro/accel bias calibration
- **Sensor Fault Detection**: Comprehensive monitoring for all sensors
- **Kidnapping Detection**: Pose jump analysis for unexpected repositioning

## Architecture

### Design Principles
- **Safety-First**: All components designed for safety-critical operation
- **Deterministic**: No dynamic allocation in real-time paths
- **Bounded**: Fixed-capacity containers and bounded queues
- **Observable**: Comprehensive diagnostics and health monitoring
- **Testable**: Extensive unit and property tests (19 test executables)

### Technology Stack
- **Language**: C++20 with MISRA/AUTOSAR-inspired coding rules
- **Build System**: CMake 3.20+ with presets for dev/safety/coverage
- **Testing**: CTest with sanitizers (AddressSanitizer, UBSanitizer)
- **Static Analysis**: clang-tidy, cppcheck, clang-format
- **CI/CD**: GitHub Actions with intelligent job orchestration
- **ROS 2 Integration**: Optional ament_cmake packaging for Jazzy

## CI/CD Pipeline

### Enterprise-Grade DevOps
- **Smart Change Detection**: Jobs run only when relevant files change
- **Security Scanning**: Trivy vulnerability detection with SARIF upload
- **SBOM Generation**: Software Bill of Materials in SPDX-JSON format
- **Performance Optimization**: Content-based caching with 30-50% better hit rates
- **Safety-Critical**: Safety guards always run regardless of changes

### Performance Impact
- Documentation-only changes: 70-80% faster
- Docker-only changes: 30-40% faster
- C++ changes: 10-20% faster
- Full CI runs: 10-15% faster

## Demo System

### ROS 2 Integration
Complete ROS 2 Jazzy + Gazebo Harmonic + Nav2 wrapper with AGV warehouse simulation:

- **safety_core_msgs**: Custom message definitions
- **safety_core_ros**: Wrapper nodes for safety envelope, supervisor, drive bridge
- **safety_core_nav2**: Nav2 behavior-tree plugin for safety gating
- **safety_core_sim**: Gazebo AGV with industrial warehouse SDF
- **safety_core_bringup**: Top-level launch with Nav2, SLAM Toolbox, RViz

### Quick Start
```bash
./setup_demo.sh  # Docker-based demo (recommended)
```

## Documentation Structure

### Core Documentation
- **README.md**: Project overview and quick start guide
- **CHANGELOG.md**: Version history and change documentation
- **CONTRIBUTING.md**: Contribution guidelines and maintainer workflow
- **AGENTS.md**: AI agent development guidelines
- **CI_DOCUMENTATION.md**: Comprehensive CI/CD pipeline documentation

### Safety Documentation
- **markdown/docs/safety_case/**: ISO 26262/ISO 13849 evidence scaffolding
  - Functional safety plan, V&V strategy, tool confidence register
  - Assumptions register, residual risk register, safety manual
  - Traceability matrix, verification protocols

### ROS 2 Documentation
- **ros2/README.md**: ROS 2 integration details and package documentation
- **ros2/LOCALIZATION.md**: Advanced localization stack guide
- **ros2/COMPREHENSIVE_DEMO.md**: Complete demonstration guide

### Docker Documentation
- **markdown/docs/DOCKER.md**: Complete Docker deployment guide
- **Dockerfile**: Multi-stage Docker build for ROS2 demo
- **docker-compose.yml**: Docker orchestration with GPU/display support

## Quality Assurance

### Testing Infrastructure
- **Unit Tests**: 19 test executables covering all components
- **Property Tests**: State machine transitions, filter invariants, safety envelopes
- **Fault Injection**: Backward-clock, NaN-burst, transport-drop tests
- **Integration Tests**: ROS2 localization and odometry stack validation
- **Automated CI Testing**: 47 automated tests validating CI/CD pipeline

### Code Quality
- **Static Analysis**: clang-tidy with cppcheck for MISRA compliance
- **Code Formatting**: clang-format with pre-commit hooks
- **Coverage**: gcovr gate (80% line, 55% branch coverage)
- **Safety Checks**: Policy guard for banned APIs, safety case artifact verification

## Build and Deployment

### Build Options
| Option | Default | Description |
|--------|---------|-------------|
| `SAFETY_CORE_ENABLE_SANITIZERS` | `ON` | Address/UB sanitizers for dev builds |
| `SAFETY_CORE_ENABLE_WERROR` | `ON` | Treat warnings as errors |
| `SAFETY_CORE_ENABLE_COVERAGE` | `OFF` | GCC coverage instrumentation |
| `SAFETY_CORE_ENABLE_AMENT` | `OFF` | ROS 2 ament_cmake integration |

### Build Presets
```bash
cmake --preset dev      # RelWithDebInfo + sanitizers + -Werror
cmake --preset safety   # Release + no sanitizers + -Werror
cmake --preset coverage # Debug + gcov instrumentation
```

### Docker Deployment
- **Multi-stage Build**: Optimized image sizes with layer caching
- **GPU Support**: NVIDIA Docker runtime for Gazebo rendering
- **Display Forwarding**: X11 server support for RViz visualization
- **Isolated Environment**: No local ROS2 installation required

## Safety-Critical Considerations

### Compliance
- **MISRA C++**: MISRA/AUTOSAR-inspired coding rules
- **ISO 26262**: Functional safety compliance scaffolding
- **ISO 13849**: Safety-related parts of control systems
- **ISO 3691-4**: Clause matrix for safety requirements

### Safety Features
- **No Dynamic Allocation**: Deterministic memory usage in real-time paths
- **Thread-Safe**: Atomic operations for concurrent access
- **Bounded Operations**: Fixed-capacity containers and queues
- **Error Handling**: Comprehensive error detection and recovery
- **Observability**: Health monitoring and diagnostic events

## Development Workflow

### Getting Started
1. **Clone Repository**: `git clone <repo-url>`
2. **Library Development**: Use CMake presets for different build configurations
3. **ROS 2 Demo**: Run `./setup_demo.sh` for Docker-based demo
4. **Testing**: Run `ctest --test-dir build --output-on-failure`
5. **Quality Gate**: Run `tools/dev/run_quality_gate.sh`

### Contributing
- Follow guidelines in CONTRIBUTING.md
- Use conventional commits for automated changelog
- Ensure all tests pass before submitting PR
- Run pre-commit hooks for code quality checks
- Update safety case documentation for safety-related changes

## Performance Characteristics

### Deterministic Operation
- **Bounded Execution**: All real-time operations have bounded execution time
- **No Heap Allocation**: Critical paths use stack/static allocation only
- **Thread-Safe**: Lock-free concurrent access where possible
- **Predictable Timing**: Deterministic state machine and executor

### Resource Efficiency
- **Small Footprint**: Minimal memory footprint for embedded deployment
- **Low Latency**: Optimized for real-time operation
- **Efficient Caching**: 30-50% cache hit rate improvement
- **Parallel Execution**: Multi-core compilation and test execution

## Future Roadmap

### Short Term
- Enhanced deployment pipeline with staging/production environments
- Performance monitoring and alerting
- Notification integration for CI failures
- Dependency scanning with Dependabot or Snyk

### Medium Term
- License scanning for compliance
- Container registry integration (GHCR)
- Hardware-in-the-loop testing in CI
- Performance regression testing

### Long Term
- Multi-environment support (dev/staging/prod)
- Automated rollback procedures
- Advanced simulation scenarios
- Real hardware integration testing

## Support and Resources

### Documentation
- **Project README**: Quick start and overview
- **CI Documentation**: Complete CI/CD pipeline guide
- **ROS 2 Documentation**: Integration and demo guides
- **Safety Documentation**: ISO compliance and safety case artifacts

### Tools and Scripts
- **setup_demo.sh**: Automated demo launcher
- **test_ci_workflow.sh**: CI validation script
- **tools/dev/**: Development tooling and quality gates

### Getting Help
- Check documentation for common issues
- Review CI summary in GitHub Actions
- Open GitHub issues for bugs or feature requests
- Review AGENTS.md for AI agent development guidelines

## License

MIT License - See LICENSE file for details

## Conclusion

Safety Autonomy Core provides a comprehensive, safety-critical foundation for autonomous robotics systems. With enterprise-grade CI/CD, extensive testing infrastructure, and compliance-ready safety documentation, it's designed for production deployment in safety-critical environments.

The combination of deterministic execution, comprehensive safety features, and modern development practices makes it suitable for AGV/AMR platforms operating in dynamic industrial environments.
