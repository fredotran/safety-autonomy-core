// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT
#pragma once

#include "safety_core/platform/sensors/odometry_sensor.hpp"

#include <nav_msgs/msg/odometry.hpp>

namespace safety_core_ros
{

    /**
     * @brief Converts ROS nav_msgs::Odometry to/from platform OdometrySample / OdometryIncrement
     *
     * OdometrySample captures position and instantaneous linear velocity.
     * OdometryIncrement captures the full twist for integration by the caller.
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
         * Maps twist.twist.linear.{x,y} and angular.z. delta_x/y/yaw are set to 0
         * because they require integration over dt (caller's responsibility).
         */
        [[nodiscard]] static safety_core::platform::sensors::OdometryIncrement
        to_increment(const nav_msgs::msg::Odometry& msg) noexcept;

        /**
         * @brief Write both sample and increment into a BufferedOdometrySensor.
         */
        static void update(safety_core::platform::sensors::BufferedOdometrySensor& sensor,
                           const nav_msgs::msg::Odometry& msg) noexcept;
    };

} // namespace safety_core_ros
