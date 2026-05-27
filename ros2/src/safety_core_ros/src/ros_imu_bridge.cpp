// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/ros_imu_bridge.hpp"

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
        sample.timestamp_ns             = static_cast<std::uint64_t>(msg.header.stamp.sec) * 1'000'000'000ULL +
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
