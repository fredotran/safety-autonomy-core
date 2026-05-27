# Sub-Project 3: Hardware Abstraction Layer — Implementation Plan

**Date**: 2026-05-27  
**Priority**: Medium  
**Branch**: main  
**Status**: PENDING

---

## Goal

Complete the `safety_core_ros_bridges` library by adding the 3 missing ROS↔platform bridge
adapters: one for IMU data, one for Odometry data, and one for the Drive Actuator.

---

## Context

### Existing bridges (already in `safety_core_ros_bridges`)
| File | Bridges |
|------|---------|
| `ros_clock.cpp` | `RosClock` — `rclcpp::Clock` → `safety_core::platform::Clock` |
| `ros_diagnostic_transport.cpp` | `RosDiagnosticTransport` — `DiagnosticTransport` impl via ROS pub |
| `ros_health_monitor.cpp` | `RosHealthMonitor` — `HealthMonitor` impl via RCLCPP logging |

### Existing platform abstractions (in `safety_autonomy_core`)
| Header | Types |
|--------|-------|
| `platform/sensors/imu_sensor.hpp` | `ImuSample`, `ImuSensor`, `BufferedImuSensor` |
| `platform/sensors/odometry_sensor.hpp` | `OdometrySample`, `OdometryIncrement`, `OdometrySensor`, `BufferedOdometrySensor` |
| `platform/actuators/drive_actuator.hpp` | `DriveCommand`, `DriveActuator`, `BufferedDriveActuator` |

### Gap
Nothing currently converts ROS messages to/from these platform types. All 3 adapters are
missing, leaving the platform layer disconnected from the ROS transport.

---

## Tasks

### Task 1 — `RosImuBridge` (SP3-T1)

**New files**:
- `include/safety_core_ros/ros_imu_bridge.hpp`
- `src/ros_imu_bridge.cpp`

**Header** (`ros_imu_bridge.hpp`):
```cpp
// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT
#pragma once

#include "safety_core/platform/sensors/imu_sensor.hpp"
#include <cstdint>
#include <sensor_msgs/msg/imu.hpp>

namespace safety_core_ros
{

/**
 * @brief Converts ROS sensor_msgs::Imu to/from platform ImuSample
 */
class RosImuBridge
{
  public:
    /**
     * @brief Convert a ROS Imu message to a platform ImuSample.
     *
     * Sets status=kValid for all finite values, kClipped if any angular velocity
     * component exceeds 30 rad/s (hardware saturation heuristic).
     */
    [[nodiscard]] static safety_core::platform::sensors::ImuSample
    to_sample(const sensor_msgs::msg::Imu& msg) noexcept;

    /**
     * @brief Write a ROS Imu message into a BufferedImuSensor.
     */
    static void update(safety_core::platform::sensors::BufferedImuSensor& sensor,
                       const sensor_msgs::msg::Imu& msg) noexcept;
};

} // namespace safety_core_ros
```

**Implementation** (`ros_imu_bridge.cpp`):
```cpp
#include "safety_core_ros/ros_imu_bridge.hpp"
#include "safety_core/platform/sensors/imu_sensor.hpp"
#include <cmath>

namespace safety_core_ros
{
    using namespace safety_core::platform::sensors;
    using namespace safety_core::platform::sensors::imu_status;

    static constexpr double kAngularVelocityClipThreshold = 30.0; // rad/s

    ImuSample RosImuBridge::to_sample(const sensor_msgs::msg::Imu& msg) noexcept
    {
        ImuSample sample;
        sample.angular_velocity_x_radps = msg.angular_velocity.x;
        sample.angular_velocity_y_radps = msg.angular_velocity.y;
        sample.angular_velocity_z_radps = msg.angular_velocity.z;
        sample.linear_accel_x_mps2      = msg.linear_acceleration.x;
        sample.linear_accel_y_mps2      = msg.linear_acceleration.y;
        sample.linear_accel_z_mps2      = msg.linear_acceleration.z;
        sample.timestamp_ns =
            static_cast<std::uint64_t>(msg.header.stamp.sec) * 1'000'000'000ULL +
            static_cast<std::uint64_t>(msg.header.stamp.nanosec);

        sample.status = kValid;
        if (std::abs(sample.angular_velocity_x_radps) > kAngularVelocityClipThreshold ||
            std::abs(sample.angular_velocity_y_radps) > kAngularVelocityClipThreshold ||
            std::abs(sample.angular_velocity_z_radps) > kAngularVelocityClipThreshold)
        {
            sample.status = static_cast<std::uint8_t>(sample.status | kClipped);
        }
        return sample;
    }

    void RosImuBridge::update(BufferedImuSensor& sensor, const sensor_msgs::msg::Imu& msg) noexcept
    {
        sensor.write(to_sample(msg));
    }

} // namespace safety_core_ros
```

---

### Task 2 — `RosOdometryBridge` (SP3-T2)

**New files**:
- `include/safety_core_ros/ros_odometry_bridge.hpp`
- `src/ros_odometry_bridge.cpp`

**Header** (`ros_odometry_bridge.hpp`):
```cpp
// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT
#pragma once

#include "safety_core/platform/sensors/odometry_sensor.hpp"
#include <nav_msgs/msg/odometry.hpp>

namespace safety_core_ros
{

/**
 * @brief Converts ROS nav_msgs::Odometry to/from platform OdometrySample / OdometryIncrement
 */
class RosOdometryBridge
{
  public:
    /**
     * @brief Convert a ROS Odometry message to a platform OdometrySample.
     * Uses pose.pose.position.x as position_m, twist.twist.linear.x as velocity_mps.
     */
    [[nodiscard]] static safety_core::platform::sensors::OdometrySample
    to_sample(const nav_msgs::msg::Odometry& msg) noexcept;

    /**
     * @brief Convert a ROS Odometry message to a platform OdometryIncrement.
     * Maps twist.twist.linear.{x,y} and angular.z directly.
     */
    [[nodiscard]] static safety_core::platform::sensors::OdometryIncrement
    to_increment(const nav_msgs::msg::Odometry& msg) noexcept;

    /**
     * @brief Write a ROS Odometry message into a BufferedOdometrySensor.
     */
    static void update(safety_core::platform::sensors::BufferedOdometrySensor& sensor,
                       const nav_msgs::msg::Odometry& msg) noexcept;
};

} // namespace safety_core_ros
```

**Implementation** (`ros_odometry_bridge.cpp`):
```cpp
#include "safety_core_ros/ros_odometry_bridge.hpp"

namespace safety_core_ros
{
    using namespace safety_core::platform::sensors;

    OdometrySample RosOdometryBridge::to_sample(const nav_msgs::msg::Odometry& msg) noexcept
    {
        OdometrySample sample;
        sample.position_m    = msg.pose.pose.position.x;
        sample.velocity_mps  = msg.twist.twist.linear.x;
        sample.timestamp_ns  =
            static_cast<std::uint64_t>(msg.header.stamp.sec) * 1'000'000'000ULL +
            static_cast<std::uint64_t>(msg.header.stamp.nanosec);
        return sample;
    }

    OdometryIncrement RosOdometryBridge::to_increment(const nav_msgs::msg::Odometry& msg) noexcept
    {
        OdometryIncrement inc;
        inc.linear_velocity_mps   = msg.twist.twist.linear.x;
        inc.angular_velocity_radps = msg.twist.twist.angular.z;
        // delta_x / delta_y / delta_yaw require dt; set to zero here
        // (caller must integrate over time if needed)
        inc.delta_x_m    = 0.0;
        inc.delta_y_m    = 0.0;
        inc.delta_yaw_rad = 0.0;
        inc.timestamp_ns =
            static_cast<std::uint64_t>(msg.header.stamp.sec) * 1'000'000'000ULL +
            static_cast<std::uint64_t>(msg.header.stamp.nanosec);
        return inc;
    }

    void RosOdometryBridge::update(BufferedOdometrySensor& sensor,
                                   const nav_msgs::msg::Odometry& msg) noexcept
    {
        sensor.write(to_sample(msg));
        sensor.write_increment(to_increment(msg));
    }

} // namespace safety_core_ros
```

---

### Task 3 — `RosDriveActuatorBridge` (SP3-T3)

**New files**:
- `include/safety_core_ros/ros_drive_actuator_bridge.hpp`
- `src/ros_drive_actuator_bridge.cpp`

**Header** (`ros_drive_actuator_bridge.hpp`):
```cpp
// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT
#pragma once

#include "safety_core/platform/actuators/drive_actuator.hpp"
#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/publisher.hpp>

namespace safety_core_ros
{

/**
 * @brief DriveActuator implementation that publishes geometry_msgs::Twist to a ROS topic.
 *
 * Bridges the platform DriveActuator abstraction to ROS transport. Inject into
 * any component that accepts a DriveActuator* to make it publish cmd_vel without
 * hard-coupling to rclcpp.
 */
class RosDriveActuatorBridge final : public safety_core::platform::actuators::DriveActuator
{
  public:
    explicit RosDriveActuatorBridge(
        rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher);

    void command(const safety_core::platform::actuators::DriveCommand& cmd) noexcept override;

    [[nodiscard]] safety_core::platform::actuators::DriveCommand
    last_command() const noexcept override;

  private:
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_;
    safety_core::platform::actuators::DriveCommand last_{};
};

} // namespace safety_core_ros
```

**Implementation** (`ros_drive_actuator_bridge.cpp`):
```cpp
#include "safety_core_ros/ros_drive_actuator_bridge.hpp"

namespace safety_core_ros
{
    using namespace safety_core::platform::actuators;

    RosDriveActuatorBridge::RosDriveActuatorBridge(
        rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher)
        : pub_(std::move(publisher))
    {}

    void RosDriveActuatorBridge::command(const DriveCommand& cmd) noexcept
    {
        last_ = cmd;
        if (pub_)
        {
            geometry_msgs::msg::Twist msg;
            msg.linear.x  = cmd.velocity_mps;
            msg.angular.z = cmd.steering_rad;
            pub_->publish(msg);
        }
    }

    DriveCommand RosDriveActuatorBridge::last_command() const noexcept
    {
        return last_;
    }

} // namespace safety_core_ros
```

---

### Task 4 — CMakeLists updates (SP3-T4)

In `ros2/src/safety_core_ros/CMakeLists.txt`:

1. Add the 3 new source files to `safety_core_ros_bridges`:
```cmake
add_library(safety_core_ros_bridges
  src/ros_clock.cpp
  src/ros_diagnostic_transport.cpp
  src/ros_health_monitor.cpp
  src/ros_imu_bridge.cpp          # NEW
  src/ros_odometry_bridge.cpp     # NEW
  src/ros_drive_actuator_bridge.cpp  # NEW
)
```

2. Add `sensor_msgs nav_msgs geometry_msgs` to `ament_target_dependencies` of `safety_core_ros_bridges`:
```cmake
ament_target_dependencies(safety_core_ros_bridges PUBLIC
  rclcpp safety_core_msgs builtin_interfaces std_msgs
  sensor_msgs nav_msgs geometry_msgs)  # add these 3
```

---

### Task 5 — Unit tests (SP3-T5)

**New file**: `ros2/src/safety_core_test/test/unit/test_ros_bridges.cpp`

Tests must cover:
- `RosImuBridge::to_sample()` — field mapping, timestamp conversion, kValid status
- `RosImuBridge::to_sample()` — kClipped status when angular velocity > 30 rad/s
- `RosOdometryBridge::to_sample()` — field mapping and timestamp conversion
- `RosOdometryBridge::to_increment()` — velocity field mapping
- `RosDriveActuatorBridge::command()` / `last_command()` — roundtrip (no ROS needed; pass nullptr publisher)
- `RosDriveActuatorBridge::is_command_fresh()` — stale check (inherited from DriveActuator)

In `ros2/src/safety_core_test/CMakeLists.txt`, add:
```cmake
# Unit Tests: ROS bridge adapters
ament_add_gtest(test_ros_bridges
  test/test_main.cpp
  test/unit/test_ros_bridges.cpp
  SKIP_LINKING_MAIN_LIBRARIES)
target_include_directories(test_ros_bridges PRIVATE
  ${CMAKE_SOURCE_DIR}/../safety_core_ros/include
  ${safety_autonomy_core_INCLUDE_DIRS})
target_link_libraries(test_ros_bridges test_fixtures safety_core_ros_bridges)
ament_target_dependencies(test_ros_bridges
  rclcpp sensor_msgs nav_msgs geometry_msgs safety_core_msgs)
```

---

## Verification

Build both packages in Docker, run all tests:
```bash
docker exec safety-autonomy-demo bash -c "
  cd /workspace/ros2_ws &&
  source /opt/ros/jazzy/setup.bash &&
  colcon build --packages-select safety_core_ros safety_core_test &&
  colcon test --packages-select safety_core_test &&
  colcon test-result --verbose"
```

All 26+ tests must pass (20 existing + 6 new bridge tests).

---

## Commit Strategy

One squashed commit:
```
feat(sp3): Add ROS↔platform sensor/actuator bridge adapters (HAL)

Add 3 new adapters to safety_core_ros_bridges completing the platform HAL:
- RosImuBridge: sensor_msgs::Imu → ImuSample / BufferedImuSensor
- RosOdometryBridge: nav_msgs::Odometry → OdometrySample + OdometryIncrement
- RosDriveActuatorBridge: DriveActuator impl publishing geometry_msgs::Twist

Adds 6 unit tests. safety_core algorithms can now be driven from ROS topics
without coupling to rclcpp by injecting the bridge adapters.
```
