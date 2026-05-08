#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/config/system_config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace safety_core::safety
{

    enum class SafetyZone : std::uint8_t
    {
        Clear = 0U,
        Warning,
        Protective,
        Emergency,
    };

    struct RobotFootprint
    {
        double length_m{0.8};
        double width_m{0.6};
        double front_overhang_m{0.1};
    };

    struct EnvelopeEvaluation
    {
        bool within_envelope{true};
        SafetyZone zone{SafetyZone::Clear};
        double stopping_distance{0.0};
        double required_clearance{0.0};
        double recommended_speed_limit_mps{0.0};
    };

    inline EnvelopeEvaluation evaluate_stop_distance(double distance_to_obstacle_m, double current_speed_mps,
                                                     double max_comfort_decel_mps2, double control_latency_s,
                                                     double safety_buffer_m = 0.1) noexcept
    {
        const double speed   = std::max(0.0, current_speed_mps);
        const double decel   = std::max(0.0, max_comfort_decel_mps2);
        const double latency = std::max(0.0, control_latency_s);

        // Simple envelope: distance covered during latency + braking distance + buffer.
        const double latency_distance = speed * latency;
        const double braking_distance =
            (decel > 0.0) ? (speed * speed) / (2.0 * decel) : std::numeric_limits<double>::infinity();
        const double required = latency_distance + braking_distance + safety_buffer_m;

        EnvelopeEvaluation eval{};
        eval.required_clearance = required;
        eval.stopping_distance  = required;
        eval.within_envelope    = distance_to_obstacle_m >= required;

        // Multi-zone evaluation
        if (distance_to_obstacle_m < safety_buffer_m)
        {
            eval.zone                        = SafetyZone::Emergency;
            eval.recommended_speed_limit_mps = 0.0;
        }
        else if (!eval.within_envelope)
        {
            eval.zone                        = SafetyZone::Protective;
            eval.recommended_speed_limit_mps = 0.0;
        }
        else if (distance_to_obstacle_m < (required * 1.5))
        {
            eval.zone = SafetyZone::Warning;
            // Recommend speed proportional to available distance
            const double available = distance_to_obstacle_m - safety_buffer_m;
            if (decel > 0.0 && available > 0.0)
            {
                eval.recommended_speed_limit_mps = std::sqrt(2.0 * decel * available * 0.7);
            }
        }
        else
        {
            eval.zone                        = SafetyZone::Clear;
            eval.recommended_speed_limit_mps = speed;
        }

        return eval;
    }

    inline EnvelopeEvaluation evaluate_stop_distance(double distance_to_obstacle_m, double current_speed_mps,
                                                     const config::MotionEnvelopeConfig& envelope) noexcept
    {
        return evaluate_stop_distance(distance_to_obstacle_m, current_speed_mps, envelope.max_comfort_decel_mps2,
                                      envelope.control_latency_s, envelope.safety_buffer_m);
    }

    inline EnvelopeEvaluation evaluate_stop_distance(double distance_to_obstacle_m, double current_speed_mps,
                                                     const config::MotionEnvelopeConfig& envelope,
                                                     const RobotFootprint& footprint) noexcept
    {
        // Account for robot geometry: reduce effective distance by front overhang
        const double effective_distance = distance_to_obstacle_m - footprint.front_overhang_m;
        return evaluate_stop_distance(effective_distance, current_speed_mps, envelope.max_comfort_decel_mps2,
                                      envelope.control_latency_s, envelope.safety_buffer_m);
    }

} // namespace safety_core::safety
