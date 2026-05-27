// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/ros_drive_actuator_bridge.hpp"

namespace safety_core_ros
{
    using namespace safety_core::platform::actuators;

    RosDriveActuatorBridge::RosDriveActuatorBridge(rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher)
        : pub_(std::move(publisher))
    {
    }

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
