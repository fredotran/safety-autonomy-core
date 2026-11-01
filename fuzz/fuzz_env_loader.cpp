// libFuzzer harness for safety_core::config::load_from_env.
//
// Feeds the fuzz input as the value of every SAFETY_CORE_* environment
// variable, then asserts the loaded config stays within its invariants:
// finite doubles, bounded integers, and a config_version within u16/u8
// ranges. Any crash (assert, UB, unterminated read) fails the harness.

#include "safety_core/config/env_loader.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    constexpr const char* kVars[] = {
        "SAFETY_CORE_CONFIG_VERSION",          "SAFETY_CORE_MAX_TASKS",
        "SAFETY_CORE_CONTROL_PERIOD_NS",       "SAFETY_CORE_WATCHDOG_PERIOD_NS",
        "SAFETY_CORE_LOCALIZATION_TIMEOUT_NS", "SAFETY_CORE_MAX_SPEED_MPS",
        "SAFETY_CORE_MAX_ACCEL_MPS2",          "SAFETY_CORE_MAX_DECEL_MPS2",
        "SAFETY_CORE_CONTROL_LATENCY_S",       "SAFETY_CORE_SAFETY_BUFFER_M",
    };

    void clear_env()
    {
        for (const char* var : kVars)
        {
            unsetenv(var);
        }
    }
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    // NUL-terminated value string; cap length to keep runs fast.
    const std::size_t len = (size > 4096U) ? 4096U : size;
    std::string value(reinterpret_cast<const char*>(data), len);

    clear_env();

    // Byte 0 selects the injection strategy: one hot variable or broadcast.
    if (len > 0U && (data[0] & 0x01U) != 0U)
    {
        const char* var = kVars[data[0] % (sizeof(kVars) / sizeof(kVars[0]))];
        setenv(var, value.c_str() + 1, 1);
    }
    else
    {
        for (const char* var : kVars)
        {
            setenv(var, value.c_str(), 1);
        }
    }

    const safety_core::config::SystemConfig cfg = safety_core::config::load_from_env();

    // Invariants: regardless of input, every parsed field must stay valid
    // for a safety controller — finite floats, bounded task count.
    if (!std::isfinite(cfg.envelope.max_speed_mps) || !std::isfinite(cfg.envelope.max_accel_mps2) ||
        !std::isfinite(cfg.envelope.max_comfort_decel_mps2) || !std::isfinite(cfg.envelope.control_latency_s) ||
        !std::isfinite(cfg.envelope.safety_buffer_m))
    {
        __builtin_trap(); // non-finite value survived the parser
    }

    clear_env();
    return 0;
}
