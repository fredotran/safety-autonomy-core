// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include "safety_core/diag/health_monitor.hpp"

#include <rclcpp/logger.hpp>
#include <rclcpp/logging.hpp>

namespace safety_core_ros
{

    // Adapts safety_core::diag::HealthMonitor to RCLCPP_* log macros, so state
    // machine transitions and latched faults appear in the rosout stream.
    class RosHealthMonitor final : public safety_core::diag::HealthMonitor
    {
      public:
        explicit RosHealthMonitor(const rclcpp::Logger& logger) noexcept : logger_(logger) {}

        void on_mode_transition(safety_core::sm::Mode from, safety_core::sm::Mode to) noexcept override;
        void on_fault_latched(std::uint16_t fault_code) noexcept override;

      private:
        rclcpp::Logger logger_;
    };

} // namespace safety_core_ros
