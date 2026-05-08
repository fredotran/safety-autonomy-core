// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/config/validation.hpp"

namespace safety_core::config
{

    ValidationResult validate(const SystemConfig& cfg, ValidationPolicy policy) noexcept
    {
        if (cfg.config_version != kCurrentSystemConfigVersion)
        {
            return ValidationResult::Error("config_version must match current schema version");
        }

        if (cfg.max_tasks == 0U)
        {
            return ValidationResult::Error("max_tasks must be non-zero");
        }

        if (cfg.timing.control_period.count() <= 0)
        {
            return ValidationResult::Error("control_period must be positive");
        }

        if (cfg.timing.watchdog_period.count() < cfg.timing.control_period.count())
        {
            return ValidationResult::Error("watchdog_period must be >= control_period");
        }

        if (cfg.timing.localization_timeout.count() <= 0)
        {
            return ValidationResult::Error("localization_timeout must be positive");
        }

        if (cfg.envelope.max_speed_mps <= 0.0 || cfg.envelope.max_accel_mps2 <= 0.0 ||
            cfg.envelope.max_comfort_decel_mps2 <= 0.0)
        {
            return ValidationResult::Error("motion envelope values must be positive");
        }

        if (cfg.envelope.control_latency_s < 0.0)
        {
            return ValidationResult::Error("control latency cannot be negative");
        }

        if (cfg.envelope.safety_buffer_m < 0.0)
        {
            return ValidationResult::Error("safety buffer cannot be negative");
        }

        if (cfg.envelope.safety_buffer_m == 0.0)
        {
            if (policy == ValidationPolicy::Strict)
            {
                return ValidationResult::Error("safety buffer must be > 0 in strict mode");
            }
            return ValidationResult::Warning("safety buffer is 0; allowed by warning policy");
        }

        return ValidationResult::Success();
    }

} // namespace safety_core::config
