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
     * Inject this into any component that accepts a DriveActuator* to produce cmd_vel
     * output without hard-coupling the component to rclcpp.
     *
     * Passing a nullptr publisher is safe — command() will still update last_command()
     * but will not attempt to publish.
     */
    class RosDriveActuatorBridge final : public safety_core::platform::actuators::DriveActuator
    {
      public:
        explicit RosDriveActuatorBridge(rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher);

        void command(const safety_core::platform::actuators::DriveCommand& cmd) noexcept override;

        [[nodiscard]] safety_core::platform::actuators::DriveCommand last_command() const noexcept override;

      private:
        rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_;
        safety_core::platform::actuators::DriveCommand last_{};
    };

} // namespace safety_core_ros
