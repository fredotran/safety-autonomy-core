// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace safety_core_ros
{

    /**
     * @brief Controls speed-limit ramping during sensor degradation/recovery
     *
     * Owns the ramp state (current limit, target limit, ramp start time) and
     * implements linear interpolation between the degraded limit (50% of max)
     * and full speed over a configurable ramp duration.
     */
    struct SpeedLimitController
    {
        // Initialise with max speed and ramp duration nanoseconds
        SpeedLimitController(double max_speed_mps, std::uint64_t ramp_ns)
            : max_speed_mps_(max_speed_mps), current_limit_mps_(max_speed_mps), target_limit_mps_(max_speed_mps),
              ramp_ns_(ramp_ns)
        {
        }

        /**
         * @brief Update and return current speed limit.
         * Call once per timer tick. Pass safe_stop_active=true when mode==SafeStop.
         */
        double update(std::uint64_t now_ns, bool safe_stop_active) noexcept
        {
            if (safe_stop_active)
            {
                current_limit_mps_ = 0.0;
                target_limit_mps_  = 0.0;
                ramp_start_ns_     = std::nullopt;
                return 0.0;
            }

            if (ramp_start_ns_.has_value())
            {
                const std::uint64_t elapsed_ns = now_ns - ramp_start_ns_.value();
                if (elapsed_ns >= ramp_ns_)
                {
                    current_limit_mps_ = target_limit_mps_;
                    ramp_start_ns_     = std::nullopt;
                }
                else
                {
                    const double degraded_limit = max_speed_mps_ * 0.5;
                    const double ramp_range     = target_limit_mps_ - degraded_limit;
                    const double fraction       = static_cast<double>(elapsed_ns) / static_cast<double>(ramp_ns_);
                    current_limit_mps_          = degraded_limit + (ramp_range * fraction);
                }
            }
            else
            {
                current_limit_mps_ = target_limit_mps_;
            }

            return current_limit_mps_;
        }

        /// Call when a sensor degrades — immediately drops to 50% of max
        void on_degraded() noexcept
        {
            target_limit_mps_  = max_speed_mps_ * 0.5;
            ramp_start_ns_     = std::nullopt;
            current_limit_mps_ = target_limit_mps_;
        }

        /// Call when all sensors have recovered — starts ramp back to max
        void on_recovered(std::uint64_t now_ns) noexcept
        {
            target_limit_mps_ = max_speed_mps_;
            ramp_start_ns_    = now_ns;
        }

        [[nodiscard]] double current_limit() const noexcept
        {
            return current_limit_mps_;
        }
        [[nodiscard]] double max_speed() const noexcept
        {
            return max_speed_mps_;
        }

      private:
        double max_speed_mps_;
        double current_limit_mps_;
        double target_limit_mps_;
        std::uint64_t ramp_ns_;
        std::optional<std::uint64_t> ramp_start_ns_;
    };

} // namespace safety_core_ros
