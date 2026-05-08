// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/diag/logging_health_monitor.hpp"

#include "safety_core/common/time.hpp"

#include <chrono>

namespace safety_core::diag
{

    LoggingHealthMonitor::LoggingHealthMonitor(std::ostream& sink) noexcept : sink_(&sink) {}

    LoggingHealthMonitor::LoggingHealthMonitor(std::ostream& sink, platform::Clock* clock) noexcept
        : sink_(&sink), clock_(clock)
    {
    }

    namespace
    {
        std::uint64_t current_timestamp_ns(platform::Clock* clock) noexcept
        {
            const auto stamp = (clock != nullptr) ? clock->now() : time::now();
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(stamp.time_since_epoch()).count());
        }
    } // namespace

    void LoggingHealthMonitor::on_mode_transition(sm::Mode from, sm::Mode to) noexcept
    {
        if (sink_ == nullptr)
        {
            return;
        }
        (*sink_) << "ts=" << current_timestamp_ns(clock_) << " mode_transition from=" << static_cast<int>(from)
                 << " to=" << static_cast<int>(to) << '\n';
    }

    void LoggingHealthMonitor::on_fault_latched(std::uint16_t fault_code) noexcept
    {
        if (sink_ == nullptr)
        {
            return;
        }
        (*sink_) << "ts=" << current_timestamp_ns(clock_) << " fault_latched code=" << fault_code << '\n';
    }

} // namespace safety_core::diag
