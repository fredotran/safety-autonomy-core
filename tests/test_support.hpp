#pragma once

#include "safety_core/config/system_config.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace safety_core::test_support
{

    inline bool check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "[FAIL] " << message << '\n';
            return false;
        }
        return true;
    }

    class CaptureTransport final : public diag::DiagnosticTransport
    {
      public:
        void publish(const diag::DiagnosticEvent& event) noexcept override
        {
            events.push_back(event);
        }

        std::vector<diag::DiagnosticEvent> events{};
    };

    inline config::SystemConfig make_motion_defaults()
    {
        using namespace std::chrono_literals;

        config::SystemConfig cfg{};
        cfg.config_version                  = config::kCurrentSystemConfigVersion;
        cfg.timing.control_period           = 100ms;
        cfg.timing.watchdog_period          = 200ms;
        cfg.timing.localization_timeout     = 2s;
        cfg.envelope.max_speed_mps          = 2.0;
        cfg.envelope.max_accel_mps2         = 1.0;
        cfg.envelope.max_comfort_decel_mps2 = 1.0;
        cfg.envelope.control_latency_s      = 0.1;
        cfg.envelope.safety_buffer_m        = 0.2;
        cfg.max_tasks                       = 4U;
        return cfg;
    }

    inline config::SystemConfig make_context_defaults()
    {
        using namespace std::chrono_literals;

        config::SystemConfig cfg{};
        cfg.config_version                  = config::kCurrentSystemConfigVersion;
        cfg.timing.control_period           = 100ms;
        cfg.timing.watchdog_period          = 200ms;
        cfg.timing.localization_timeout     = 2s;
        cfg.envelope.max_speed_mps          = 1.2;
        cfg.envelope.max_accel_mps2         = 0.6;
        cfg.envelope.max_comfort_decel_mps2 = 0.8;
        cfg.envelope.control_latency_s      = 0.1;
        cfg.envelope.safety_buffer_m        = 0.2;
        cfg.max_tasks                       = 4U;
        return cfg;
    }

    inline void clear_config_env_vars()
    {
        unsetenv("SAFETY_CORE_CONFIG_VERSION");
        unsetenv("SAFETY_CORE_MAX_TASKS");
        unsetenv("SAFETY_CORE_CONTROL_PERIOD_NS");
        unsetenv("SAFETY_CORE_WATCHDOG_PERIOD_NS");
        unsetenv("SAFETY_CORE_LOCALIZATION_TIMEOUT_NS");
        unsetenv("SAFETY_CORE_MAX_SPEED_MPS");
        unsetenv("SAFETY_CORE_MAX_ACCEL_MPS2");
        unsetenv("SAFETY_CORE_MAX_DECEL_MPS2");
        unsetenv("SAFETY_CORE_CONTROL_LATENCY_S");
        unsetenv("SAFETY_CORE_SAFETY_BUFFER_M");
    }

} // namespace safety_core::test_support
