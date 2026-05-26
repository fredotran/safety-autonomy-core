# Testing Infrastructure Design Spec

**Date**: 2026-05-26
**Sub-Project**: 1 of 5 (Testing Infrastructure)
**Approach**: Comprehensive Testing (B)
**Status**: Ready for Implementation

---

## 1. Overview

Establish a robust testing framework for the safety-autonomy-core ROS2 codebase that supports unit, component, and integration testing. This addresses the critical gap of zero test coverage in a safety-critical system.

### Goals
- Production readiness: Verify safety-critical logic
- Team scalability: Enable confident refactoring
- Long-term sustainability: Prevent regressions
- Compliance: Support ISO 26262/MISRA evidence requirements

---

## 2. Architecture

### Package Structure
```
ros2/src/safety_core_test/
├── CMakeLists.txt
├── package.xml
├── test/
│   ├── test_main.cpp
│   ├── unit/
│   │   ├── test_state_machine.cpp
│   │   ├── test_sensor_health.cpp
│   │   └── test_math_utils.cpp
│   ├── component/
│   │   ├── test_sensor_monitor.cpp
│   │   ├── test_slip_detector.cpp
│   │   └── test_gps_adapter.cpp
│   └── integration/
│       ├── test_safety_stack.cpp
│       └── test_localization_stack.cpp
└── fixtures/
    ├── mock_clock.hpp
    ├── mock_health_monitor.hpp
    ├── test_scenarios.hpp
    └── sensor_data/
```

### Build Integration
- Add `BUILD_TESTING` option to `safety_core_ros` CMakeLists.txt
- Tests run via `colcon test` or `ctest`
- Docker container supports headless test execution

---

## 3. Unit Tests (Core Logic)

### Components
| Component | File Under Test | Key Scenarios |
|-----------|-----------------|---------------|
| State Machine | `safety_core` lib (ModeStateMachine) | All mode transitions, fault latching, clearing |
| Sensor Health | `sensor_monitor_node` logic | Rate classification, threshold boundaries, staleness |
| Math Utils | `math_utils.hpp` | Velocity integration, exponential decay |
| Time Utils | `time_utils.hpp` | Staleness detection, age calculation, rollover |

### Coverage Target: 90% line coverage

---

## 4. Component Tests (ROS Nodes)

### Approach: ROS2 launch testing with `launch_testing`

| Node | Test Scenarios |
|------|----------------|
| sensor_monitor_node | Mock sensor msgs, startup grace, thresholds |
| slip_detector_node | Synchronized odom+IMU, slip thresholds |
| gps_covariance_adapter | Quality-based covariance scaling |
| safety_supervisor_node | Mode transitions, fault latching, recovery timeout |

---

## 5. Integration Tests

| Stack | Test Scenarios |
|-------|----------------|
| Safety Stack | Sensor failure -> SafeStop, recovery -> Moving, speed limits |
| Localization | GPS adaptation -> EKF propagation, degraded GPS fusion |

---

## 6. Test Fixtures & Mocks

### Reusable Infrastructure
- `MockClock` - Deterministic time control
- `MockHealthMonitor` - Diagnostic event verification
- `TestScenarios` - Predefined sensor failure/recovery sequences
- `MessageBuilders` - Helper functions for test messages

---

## 7. CI Integration

### GitHub Actions
- New `test` job (runs after `build`)
- Docker-based execution
- Test result artifacts
- Coverage reporting (target: 80% initially)

### Commands
```bash
# Unit tests only
colcon test --packages-select safety_core_test --ctest-args -R unit

# Component tests
colcon test --packages-select safety_core_test --ctest-args -R component

# Integration tests
colcon test --packages-select safety_core_test --ctest-args -R integration

# All tests
colcon test --packages-select safety_core_test
```

---

## 8. Acceptance Criteria

- [ ] All unit tests pass (state machine, sensor health, math/time utils)
- [ ] All component tests pass (4 nodes tested in isolation)
- [ ] Integration tests pass (2 stacks tested end-to-end)
- [ ] CI pipeline runs tests automatically
- [ ] Coverage report generated (target: 80% overall)
- [ ] Tests run in Docker container (headless)

---

## 9. Dependencies

### New Package Dependencies
- `ament_cmake_test` (ROS2 testing)
- `launch_testing` (ROS2 launch tests)
- `gtest` / `gmock` (unit testing)
- `gcov` / `lcov` (coverage)

### No External Dependencies
All testing infrastructure uses existing ROS2 tooling.

---

## 10. Risk Mitigation

| Risk | Mitigation |
|------|------------|
| ROS2 test framework complexity | Start with simple component tests, build up |
| Headless Docker execution | Pre-configured container with xvfb |
| Flaky integration tests | Use deterministic mocks, avoid real-time dependencies |
| Coverage gaps | Prioritize safety-critical paths first |

---

**End of Design Spec**
