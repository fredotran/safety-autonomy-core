// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/config/validation.hpp"

#include <cmath>

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

        // Additional safety: prevent unreasonably large task counts
        if (cfg.max_tasks > 100U)
        {
            return ValidationResult::Error("max_tasks exceeds safe maximum (100)");
        }

        if (cfg.timing.control_period.count() <= 0)
        {
            return ValidationResult::Error("control_period must be positive");
        }

        // Safety: prevent unreasonably small control periods (could cause CPU overload)
        constexpr std::chrono::nanoseconds kMinControlPeriod(1000000); // 1ms minimum
        if (cfg.timing.control_period < kMinControlPeriod)
        {
            return ValidationResult::Error("control_period too small (minimum 1ms)");
        }

        if (cfg.timing.watchdog_period.count() < cfg.timing.control_period.count())
        {
            return ValidationResult::Error("watchdog_period must be >= control_period");
        }

        if (cfg.timing.localization_timeout.count() <= 0)
        {
            return ValidationResult::Error("localization_timeout must be positive");
        }

        // Validate floating-point values are finite and reasonable
        if (!std::isfinite(cfg.envelope.max_speed_mps) || !std::isfinite(cfg.envelope.max_accel_mps2) ||
            !std::isfinite(cfg.envelope.max_comfort_decel_mps2) || !std::isfinite(cfg.envelope.control_latency_s) ||
            !std::isfinite(cfg.envelope.safety_buffer_m))
        {
            return ValidationResult::Error("motion envelope values must be finite");
        }

        if (cfg.envelope.max_speed_mps <= 0.0 || cfg.envelope.max_accel_mps2 <= 0.0 ||
            cfg.envelope.max_comfort_decel_mps2 <= 0.0)
        {
            return ValidationResult::Error("motion envelope values must be positive");
        }

        // Safety: prevent unreasonably high speeds/accelerations
        if (cfg.envelope.max_speed_mps > 100.0)
        {
            return ValidationResult::Error("max_speed_mps exceeds safe maximum (100 m/s)");
        }
        if (cfg.envelope.max_accel_mps2 > 50.0)
        {
            return ValidationResult::Error("max_accel_mps2 exceeds safe maximum (50 m/s²)");
        }

        if (cfg.envelope.control_latency_s < 0.0)
        {
            return ValidationResult::Error("control latency cannot be negative");
        }

        // Safety: prevent unreasonably high latency
        if (cfg.envelope.control_latency_s > 1.0)
        {
            return ValidationResult::Error("control latency exceeds safe maximum (1.0s)");
        }

        if (cfg.envelope.safety_buffer_m < 0.0)
        {
            return ValidationResult::Error("safety buffer cannot be negative");
        }

        // Safety: prevent unreasonably large safety buffers
        if (cfg.envelope.safety_buffer_m > 100.0)
        {
            return ValidationResult::Error("safety buffer exceeds safe maximum (100m)");
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
