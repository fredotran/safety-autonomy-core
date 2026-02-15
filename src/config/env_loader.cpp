#include "safety_core/config/env_loader.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace safety_core::config
{
    namespace
    {

        std::uint64_t read_uint64(const char* name, std::uint64_t fallback) noexcept
        {
            if (const char* value = std::getenv(name))
            {
                errno                           = 0;
                char* end                       = nullptr;
                const unsigned long long parsed = std::strtoull(value, &end, 10);
                if ((end != value) && (end != nullptr) && (*end == '\0') && (errno != ERANGE))
                {
                    if (parsed <= static_cast<unsigned long long>(std::numeric_limits<std::uint64_t>::max()))
                    {
                        return static_cast<std::uint64_t>(parsed);
                    }
                }
            }
            return fallback;
        }

        std::uint64_t read_uint64_bounded(const char* name, std::uint64_t fallback, std::uint64_t min_value,
                                          std::uint64_t max_value) noexcept
        {
            const std::uint64_t parsed = read_uint64(name, fallback);
            if ((parsed < min_value) || (parsed > max_value))
            {
                return fallback;
            }
            return parsed;
        }

        double read_double(const char* name, double fallback) noexcept
        {
            if (const char* value = std::getenv(name))
            {
                errno               = 0;
                char* end           = nullptr;
                const double parsed = std::strtod(value, &end);
                if ((end != value) && (end != nullptr) && (*end == '\0') && (errno != ERANGE) && std::isfinite(parsed))
                {
                    return parsed;
                }
            }
            return fallback;
        }

    } // namespace

    SystemConfig load_from_env(SystemConfig defaults) noexcept
    {
        SystemConfig cfg = defaults;

        cfg.config_version = static_cast<std::uint16_t>(
            read_uint64_bounded("SAFETY_CORE_CONFIG_VERSION", static_cast<std::uint64_t>(cfg.config_version), 0U,
                                static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max())));

        cfg.max_tasks = static_cast<std::uint8_t>(
            read_uint64_bounded("SAFETY_CORE_MAX_TASKS", cfg.max_tasks, 0U,
                                static_cast<std::uint64_t>(std::numeric_limits<std::uint8_t>::max())));

        const auto control_ns =
            read_uint64("SAFETY_CORE_CONTROL_PERIOD_NS", static_cast<std::uint64_t>(cfg.timing.control_period.count()));
        const auto watchdog_ns     = read_uint64("SAFETY_CORE_WATCHDOG_PERIOD_NS",
                                                 static_cast<std::uint64_t>(cfg.timing.watchdog_period.count()));
        const auto localization_ns = read_uint64("SAFETY_CORE_LOCALIZATION_TIMEOUT_NS",
                                                 static_cast<std::uint64_t>(cfg.timing.localization_timeout.count()));

        cfg.timing.control_period       = time::Duration(control_ns);
        cfg.timing.watchdog_period      = time::Duration(watchdog_ns);
        cfg.timing.localization_timeout = time::Duration(localization_ns);

        cfg.envelope.max_speed_mps  = read_double("SAFETY_CORE_MAX_SPEED_MPS", cfg.envelope.max_speed_mps);
        cfg.envelope.max_accel_mps2 = read_double("SAFETY_CORE_MAX_ACCEL_MPS2", cfg.envelope.max_accel_mps2);
        cfg.envelope.max_comfort_decel_mps2 =
            read_double("SAFETY_CORE_MAX_DECEL_MPS2", cfg.envelope.max_comfort_decel_mps2);
        cfg.envelope.control_latency_s = read_double("SAFETY_CORE_CONTROL_LATENCY_S", cfg.envelope.control_latency_s);
        cfg.envelope.safety_buffer_m   = read_double("SAFETY_CORE_SAFETY_BUFFER_M", cfg.envelope.safety_buffer_m);

        return cfg;
    }

} // namespace safety_core::config
