#pragma once

#include "safety_core/common/time.hpp"

#include <cstdint>

namespace safety_core::config
{

    struct TimingConfig
    {
        time::Duration control_period;
        time::Duration watchdog_period;
        time::Duration localization_timeout;
    };

    struct MotionEnvelopeConfig
    {
        double max_speed_mps;
        double max_accel_mps2;
        double max_comfort_decel_mps2;
        double control_latency_s;
        double safety_buffer_m;
    };

    struct SystemConfig
    {
        TimingConfig timing;
        MotionEnvelopeConfig envelope;
        std::uint8_t max_tasks;
    };

} // namespace safety_core::config
