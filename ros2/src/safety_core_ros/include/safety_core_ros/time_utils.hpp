// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <cstdint>
#include <rclcpp/clock.hpp>

namespace safety_core_ros
{

    /**
     * @brief Utility class for time-related operations
     *
     * Provides helper functions for common time operations used throughout
     * the safety system, including conversions between different time formats
     * and time-based calculations.
     */
    class TimeUtils
    {
      public:
        /**
         * @brief Convert seconds to nanoseconds
         * @param seconds Time in seconds
         * @return Time in nanoseconds
         */
        static constexpr std::uint64_t seconds_to_nanoseconds(double seconds) noexcept
        {
            return static_cast<std::uint64_t>(seconds * 1e9);
        }

        /**
         * @brief Convert nanoseconds to seconds
         * @param nanoseconds Time in nanoseconds
         * @return Time in seconds
         */
        static constexpr double nanoseconds_to_seconds(std::uint64_t nanoseconds) noexcept
        {
            return static_cast<double>(nanoseconds) * 1e-9;
        }

        /**
         * @brief Get current time in nanoseconds from a clock
         * @param clock ROS 2 clock to use
         * @return Current time in nanoseconds
         */
        static std::uint64_t now_nanoseconds(const rclcpp::Clock::ConstSharedPtr& clock)
        {
            return static_cast<std::uint64_t>(clock->now().nanoseconds());
        }

        /**
         * @brief Calculate age of a timestamp in nanoseconds
         * @param timestamp_ns Timestamp in nanoseconds
         * @param current_time_ns Current time in nanoseconds
         * @return Age in nanoseconds
         */
        static constexpr std::uint64_t calculate_age_ns(std::uint64_t timestamp_ns,
                                                        std::uint64_t current_time_ns) noexcept
        {
            return current_time_ns - timestamp_ns;
        }

        /**
         * @brief Check if a timestamp is stale based on timeout
         * @param timestamp_ns Timestamp to check
         * @param current_time_ns Current time in nanoseconds
         * @param timeout_ns Timeout threshold in nanoseconds
         * @return true if timestamp is stale, false otherwise
         */
        static constexpr bool is_stale(std::uint64_t timestamp_ns, std::uint64_t current_time_ns,
                                       std::uint64_t timeout_ns) noexcept
        {
            return calculate_age_ns(timestamp_ns, current_time_ns) > timeout_ns;
        }
    };

} // namespace safety_core_ros
