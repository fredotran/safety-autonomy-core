// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT
#pragma once

#include "safety_core/platform/sensors/imu_sensor.hpp"

#include <sensor_msgs/msg/imu.hpp>

namespace safety_core_ros
{

    /**
     * @brief Converts ROS sensor_msgs::Imu to/from platform ImuSample
     *
     * Bridges the ROS transport layer to the platform sensor abstraction.
     * All conversion functions are static and noexcept for use in real-time callbacks.
     */
    class RosImuBridge
    {
      public:
        /**
         * @brief Convert a ROS Imu message to a platform ImuSample.
         *
         * Maps angular velocity and linear acceleration directly.
         * Sets status=kValid for all messages; adds kClipped flag if any angular
         * velocity component exceeds 30 rad/s (hardware saturation heuristic).
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
