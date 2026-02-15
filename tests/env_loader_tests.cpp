#include "safety_core/config/env_loader.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

    bool check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "[FAIL] " << message << '\n';
            return false;
        }
        return true;
    }

    safety_core::config::SystemConfig make_defaults()
    {
        using namespace std::chrono_literals;

        safety_core::config::SystemConfig cfg{};
        cfg.config_version                  = 2U;
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

    void clear_env_vars()
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

} // namespace

int main()
{
    using safety_core::config::load_from_env;

    bool ok                                          = true;
    const safety_core::config::SystemConfig defaults = make_defaults();

    clear_env_vars();

    // Leading space and plus sign should parse.
    setenv("SAFETY_CORE_MAX_TASKS", "+7", 1);
    setenv("SAFETY_CORE_MAX_SPEED_MPS", " 2.5", 1);
    auto cfg = load_from_env(defaults);
    ok &= check(cfg.max_tasks == 7U, "plus sign integer should parse");
    ok &= check(std::abs(cfg.envelope.max_speed_mps - 2.5) < 1e-9, "leading-space double should parse");

    // Trailing spaces should be rejected (fallback to default).
    setenv("SAFETY_CORE_MAX_TASKS", "12 ", 1);
    setenv("SAFETY_CORE_MAX_SPEED_MPS", "3.0 ", 1);
    cfg = load_from_env(defaults);
    ok &= check(cfg.max_tasks == defaults.max_tasks, "trailing-space integer should fallback");
    ok &= check(std::abs(cfg.envelope.max_speed_mps - defaults.envelope.max_speed_mps) < 1e-9,
                "trailing-space double should fallback");

    // Negative sign on bounded unsigned input should fallback.
    setenv("SAFETY_CORE_MAX_TASKS", "-1", 1);
    cfg = load_from_env(defaults);
    ok &= check(cfg.max_tasks == defaults.max_tasks, "negative unsigned integer should fallback");

    // Boundary edges for uint8/uint16 should accept max and reject overflow.
    setenv("SAFETY_CORE_MAX_TASKS", "255", 1);
    setenv("SAFETY_CORE_CONFIG_VERSION", "65535", 1);
    cfg = load_from_env(defaults);
    ok &= check(cfg.max_tasks == 255U, "uint8 max should parse");
    ok &= check(cfg.config_version == 65535U, "uint16 max should parse");

    setenv("SAFETY_CORE_MAX_TASKS", "256", 1);
    setenv("SAFETY_CORE_CONFIG_VERSION", "65536", 1);
    cfg = load_from_env(defaults);
    ok &= check(cfg.max_tasks == defaults.max_tasks, "uint8 overflow should fallback");
    ok &= check(cfg.config_version == defaults.config_version, "uint16 overflow should fallback");

    // Huge overflow input should fallback.
    setenv("SAFETY_CORE_MAX_TASKS", "999999999999999999999999999999", 1);
    cfg = load_from_env(defaults);
    ok &= check(cfg.max_tasks == defaults.max_tasks, "ERANGE integer overflow should fallback");

    // nan/inf should fallback for doubles.
    setenv("SAFETY_CORE_MAX_SPEED_MPS", "nan", 1);
    cfg = load_from_env(defaults);
    ok &= check(std::abs(cfg.envelope.max_speed_mps - defaults.envelope.max_speed_mps) < 1e-9, "nan should fallback");

    setenv("SAFETY_CORE_MAX_SPEED_MPS", "inf", 1);
    cfg = load_from_env(defaults);
    ok &= check(std::abs(cfg.envelope.max_speed_mps - defaults.envelope.max_speed_mps) < 1e-9, "inf should fallback");

    setenv("SAFETY_CORE_MAX_SPEED_MPS", "-inf", 1);
    cfg = load_from_env(defaults);
    ok &= check(std::abs(cfg.envelope.max_speed_mps - defaults.envelope.max_speed_mps) < 1e-9, "-inf should fallback");

    // Property-style malformed integer parsing: any mixed-token input must fallback.
    for (unsigned i = 0U; i < 128U; ++i)
    {
        const std::string malformed = std::to_string(i) + "x" + std::to_string(127U - i);
        setenv("SAFETY_CORE_MAX_TASKS", malformed.c_str(), 1);
        cfg = load_from_env(defaults);
        ok &= check(cfg.max_tasks == defaults.max_tasks, "mixed-token max_tasks input should fallback to default");
    }

    // Property-style malformed double parsing: non-terminal junk must fallback.
    for (unsigned i = 1U; i <= 128U; ++i)
    {
        const std::string malformed = "1." + std::to_string(i) + "e2junk";
        setenv("SAFETY_CORE_MAX_SPEED_MPS", malformed.c_str(), 1);
        cfg = load_from_env(defaults);
        ok &= check(std::abs(cfg.envelope.max_speed_mps - defaults.envelope.max_speed_mps) < 1e-9,
                    "mixed-token max_speed input should fallback to default");
    }

    clear_env_vars();

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] env loader tests" << '\n';
    return 0;
}
