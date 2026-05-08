// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/ros_health_monitor.hpp"

#include "safety_core/state_machine/state_machine.hpp"

namespace safety_core_ros
{
    namespace
    {

        const char* mode_to_string(safety_core::sm::Mode mode) noexcept
        {
            using safety_core::sm::Mode;
            switch (mode)
            {
            case Mode::Init:
                return "Init";
            case Mode::Idle:
                return "Idle";
            case Mode::Moving:
                return "Moving";
            case Mode::Degraded:
                return "Degraded";
            case Mode::AvoidingObstacle:
                return "AvoidingObstacle";
            case Mode::LocalizationLost:
                return "LocalizationLost";
            case Mode::Docking:
                return "Docking";
            case Mode::SafeStop:
                return "SafeStop";
            }
            return "Unknown";
        }

    } // namespace

    void RosHealthMonitor::on_mode_transition(safety_core::sm::Mode from, safety_core::sm::Mode to) noexcept
    {
        RCLCPP_INFO(logger_, "[safety_core] mode %s -> %s", mode_to_string(from), mode_to_string(to));
    }

    void RosHealthMonitor::on_fault_latched(std::uint16_t fault_code) noexcept
    {
        RCLCPP_ERROR(logger_, "[safety_core] fault latched: code=0x%04X", fault_code);
    }

} // namespace safety_core_ros
