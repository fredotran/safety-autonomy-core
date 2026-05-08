// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/ros_clock.hpp"

#include <chrono>

namespace safety_core_ros
{

    safety_core::time::TimePoint RosClock::now() const noexcept
    {
        // rclcpp::Clock::now() can throw on internal failures; we treat any
        // exception as a hard fault and return time_zero so safety logic detects
        // a regression. In practice this only matters during shutdown.
        try
        {
            const auto stamp = clock_->now();
            const std::chrono::nanoseconds ns(stamp.nanoseconds());
            return safety_core::time::TimePoint(ns);
        }
        catch (...)
        {
            return safety_core::time::TimePoint{};
        }
    }

} // namespace safety_core_ros
