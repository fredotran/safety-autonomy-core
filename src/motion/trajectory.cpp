// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/motion/trajectory.hpp"

#include "safety_core/safety/safety_envelope.hpp"

#include <algorithm>
#include <cmath>

namespace safety_core::motion
{

    namespace
    {
        constexpr double kNumericEpsilon = 1e-12;
    }

    TrajectoryValidationResult TrajectoryValidator::validate(const TrajectoryPoint* points,
                                                             std::size_t count) const noexcept
    {
        if (points == nullptr)
        {
            return {false, TrajectoryViolation::NullInput, 0U};
        }
        if (count == 0U)
        {
            return {false, TrajectoryViolation::EmptyTrajectory, 0U};
        }

        const double max_speed = (config_ != nullptr) ? std::max(0.0, config_->envelope.max_speed_mps) : 0.0;
        const double max_accel = (config_ != nullptr) ? std::max(0.0, config_->envelope.max_accel_mps2) : 0.0;

        for (std::size_t i = 0U; i < count; ++i)
        {
            const auto& p = points[i];

            if (!std::isfinite(p.t_s) || !std::isfinite(p.speed_mps) || !std::isfinite(p.accel_mps2) ||
                !std::isfinite(p.jerk_mps3) || !std::isfinite(p.distance_m))
            {
                // Non-finite samples are treated as invalid input and surfaced through NullInput.
                return {false, TrajectoryViolation::NullInput, i};
            }

            if (i > 0U)
            {
                const auto& prev = points[i - 1U];
                if (p.t_s <= prev.t_s)
                {
                    return {false, TrajectoryViolation::NonMonotonicTime, i};
                }
                if (p.distance_m + kNumericEpsilon < prev.distance_m)
                {
                    return {false, TrajectoryViolation::NonMonotonicDistance, i};
                }
            }

            if ((max_speed > 0.0) && (std::abs(p.speed_mps) > (max_speed + kNumericEpsilon)))
            {
                return {false, TrajectoryViolation::SpeedOutOfBounds, i};
            }
            if ((max_accel > 0.0) && (std::abs(p.accel_mps2) > (max_accel + kNumericEpsilon)))
            {
                return {false, TrajectoryViolation::AccelOutOfBounds, i};
            }
            if (std::abs(p.jerk_mps3) > (max_jerk_mps3_ + kNumericEpsilon))
            {
                return {false, TrajectoryViolation::JerkOutOfBounds, i};
            }

            if ((config_ != nullptr) && (obstacle_distance_m_ >= 0.0))
            {
                const double remaining_distance = obstacle_distance_m_ - p.distance_m;
                const auto envelope_eval =
                    safety::evaluate_stop_distance(remaining_distance, p.speed_mps, config_->envelope);
                if (!envelope_eval.within_envelope)
                {
                    return {false, TrajectoryViolation::EnvelopeViolation, i};
                }
            }
        }

        return {true, TrajectoryViolation::None, 0U};
    }

    bool generate_emergency_stop_profile(double initial_speed_mps, double max_decel_mps2, double dt_s,
                                         TrajectoryPoint* out_points, std::size_t capacity,
                                         std::size_t& out_count) noexcept
    {
        out_count = 0U;
        if ((out_points == nullptr) || (capacity == 0U) || !std::isfinite(initial_speed_mps) ||
            !std::isfinite(max_decel_mps2) || !std::isfinite(dt_s) || (dt_s <= 0.0) || (max_decel_mps2 <= 0.0))
        {
            return false;
        }

        // Commentary: closed-form decel update keeps deterministic primitive generation without dynamic allocation.
        double t = 0.0;
        double v = std::max(0.0, initial_speed_mps);
        double x = 0.0;

        while ((v > 0.0) && (out_count < capacity))
        {
            const double next_v = std::max(0.0, v - (max_decel_mps2 * dt_s));
            const double accel  = (next_v - v) / dt_s;
            const double step_d = ((v + next_v) * 0.5) * dt_s;

            out_points[out_count] = TrajectoryPoint{t, v, accel, 0.0, x};
            ++out_count;

            t += dt_s;
            x += step_d;
            v = next_v;
        }

        if (out_count >= capacity)
        {
            return false;
        }

        out_points[out_count] = TrajectoryPoint{t, 0.0, 0.0, 0.0, x};
        ++out_count;
        return true;
    }

    bool generate_jerk_limited_stop_profile(double initial_speed_mps, double max_decel_mps2, double max_jerk_mps3,
                                            double dt_s, TrajectoryPoint* out_points, std::size_t capacity,
                                            std::size_t& out_count) noexcept
    {
        out_count = 0U;
        if ((out_points == nullptr) || (capacity == 0U) || !std::isfinite(initial_speed_mps) ||
            !std::isfinite(max_decel_mps2) || !std::isfinite(max_jerk_mps3) || !std::isfinite(dt_s) || (dt_s <= 0.0) ||
            (max_decel_mps2 <= 0.0) || (max_jerk_mps3 <= 0.0))
        {
            return false;
        }

        double t    = 0.0;
        double v    = std::max(0.0, initial_speed_mps);
        double a    = 0.0;
        double x    = 0.0;
        double jerk = 0.0;

        while ((v > 0.0) && (out_count < capacity))
        {
            // Phase 1: Ramp up deceleration (jerk = -max_jerk)
            // Phase 2: Constant deceleration (jerk = 0)
            // Phase 3: Ramp down deceleration (jerk = +max_jerk) as speed approaches zero
            if (a > -max_decel_mps2)
            {
                // Ramp up deceleration
                jerk = -max_jerk_mps3;
                a += jerk * dt_s;
                if (a < -max_decel_mps2)
                {
                    a    = -max_decel_mps2;
                    jerk = 0.0;
                }
            }
            else
            {
                // At max deceleration — check if we need to ramp down
                // Time to stop at current decel: t_stop = v / |a|
                // Time to ramp decel to zero: t_ramp = |a| / jerk
                const double t_stop = (std::abs(a) > 1e-9) ? (v / std::abs(a)) : 0.0;
                const double t_ramp = std::abs(a) / max_jerk_mps3;
                if (t_stop <= t_ramp)
                {
                    jerk = max_jerk_mps3;
                    a += jerk * dt_s;
                    if (a > 0.0)
                    {
                        a    = 0.0;
                        jerk = 0.0;
                    }
                }
                else
                {
                    jerk = 0.0;
                }
            }

            out_points[out_count] = TrajectoryPoint{t, v, a, jerk, x};
            ++out_count;

            const double next_v = std::max(0.0, v + (a * dt_s));
            const double step_d = ((v + next_v) * 0.5) * dt_s;

            t += dt_s;
            x += step_d;
            v = next_v;
        }

        if (out_count >= capacity)
        {
            return false;
        }

        out_points[out_count] = TrajectoryPoint{t, 0.0, 0.0, 0.0, x};
        ++out_count;
        return true;
    }

} // namespace safety_core::motion
