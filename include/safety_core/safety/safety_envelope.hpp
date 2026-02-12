#pragma once

#include "safety_core/result.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace safety_core::safety
{

    struct EnvelopeEvaluation
    {
        bool within_envelope{true};
        double stopping_distance{0.0};
        double required_clearance{0.0};
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
        return eval;
    }

} // namespace safety_core::safety
