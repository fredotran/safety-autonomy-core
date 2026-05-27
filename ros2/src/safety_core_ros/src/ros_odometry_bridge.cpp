// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/ros_odometry_bridge.hpp"

namespace safety_core_ros
{
    using namespace safety_core::platform::sensors;

    OdometrySample RosOdometryBridge::to_sample(const nav_msgs::msg::Odometry& msg) noexcept
    {
        OdometrySample sample;
        sample.position_m   = msg.pose.pose.position.x;
        sample.velocity_mps = msg.twist.twist.linear.x;
        sample.timestamp_ns = static_cast<std::uint64_t>(msg.header.stamp.sec) * 1'000'000'000ULL +
                              static_cast<std::uint64_t>(msg.header.stamp.nanosec);
        return sample;
    }

    OdometryIncrement RosOdometryBridge::to_increment(const nav_msgs::msg::Odometry& msg) noexcept
    {
        OdometryIncrement inc;
        inc.linear_velocity_mps    = msg.twist.twist.linear.x;
        inc.angular_velocity_radps = msg.twist.twist.angular.z;
        // delta_x / delta_y / delta_yaw require dt — set to zero; caller integrates
        inc.delta_x_m     = 0.0;
        inc.delta_y_m     = 0.0;
        inc.delta_yaw_rad = 0.0;
        inc.timestamp_ns  = static_cast<std::uint64_t>(msg.header.stamp.sec) * 1'000'000'000ULL +
                           static_cast<std::uint64_t>(msg.header.stamp.nanosec);
        return inc;
    }

    void RosOdometryBridge::update(BufferedOdometrySensor& sensor, const nav_msgs::msg::Odometry& msg) noexcept
    {
        sensor.write(to_sample(msg));
        sensor.write_increment(to_increment(msg));
    }

} // namespace safety_core_ros
