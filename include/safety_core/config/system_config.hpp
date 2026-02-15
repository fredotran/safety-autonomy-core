#pragma once

#include "safety_core/common/time.hpp"

#include <cstdint>

namespace safety_core::config
{

    constexpr std::uint16_t kLegacySystemConfigVersion  = 1U;
    constexpr std::uint16_t kCurrentSystemConfigVersion = 2U;

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
        std::uint16_t config_version{kCurrentSystemConfigVersion};
    };

} // namespace safety_core::config
