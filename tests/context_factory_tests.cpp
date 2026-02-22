#include "safety_core/diag/topics.hpp"
#include "safety_core/platform/manual_clock.hpp"
#include "safety_core/safety/safety_supervisor.hpp"
#include "safety_core/system/context_factory.hpp"
#include "test_support.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>

namespace
{
    using safety_core::test_support::check;
    using CaptureTransport = safety_core::test_support::CaptureTransport;

    safety_core::config::SystemConfig make_defaults()
    {
        using namespace std::chrono_literals;

        safety_core::config::SystemConfig cfg{};
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
    using safety_core::system::ContextFactoryOptions;
    using safety_core::system::ContextWithConfig;

    bool ok = true;
    clear_env_vars();

    safety_core::platform::ManualClock manual_clock;
    CaptureTransport transport;
    safety_core::safety::SafetySupervisor supervisor;

    // Happy-path: env values override defaults and produce a valid context.
    setenv("SAFETY_CORE_CONFIG_VERSION", "1", 1);
    setenv("SAFETY_CORE_MAX_TASKS", "6", 1);
    setenv("SAFETY_CORE_CONTROL_PERIOD_NS", "50000000", 1);
    setenv("SAFETY_CORE_WATCHDOG_PERIOD_NS", "100000000", 1);
    setenv("SAFETY_CORE_LOCALIZATION_TIMEOUT_NS", "3000000000", 1);
    setenv("SAFETY_CORE_MAX_SPEED_MPS", "2.0", 1);

    ContextWithConfig built{};
    ContextFactoryOptions options{};
    options.defaults             = make_defaults();
    options.clock                = &manual_clock;
    options.diagnostic_transport = &transport;
    options.safety_supervisor    = &supervisor;

    const auto result = safety_core::system::build_context(built, options);
    ok &= check(result.ok(), "build_context should succeed with valid env");
    ok &= check(built.context.config == &built.config, "context config pointer should reference owned config");
    ok &= check(built.context.clock == &manual_clock, "context should keep configured clock");
    ok &= check(built.context.safety_supervisor == &supervisor, "context should keep configured safety supervisor");
    ok &= check(built.config.max_tasks == 6U, "env override max_tasks should apply");
    ok &= check(built.config.timing.control_period.count() == 50000000, "env override control period should apply");
    ok &= check(built.config.config_version == safety_core::config::kCurrentSystemConfigVersion,
                "legacy config version should migrate to current schema");
    bool saw_migration_event = false;
    for (const auto& event : transport.events)
    {
        if (event.topic_view() == safety_core::diag::topic::kConfigMigration)
        {
            saw_migration_event = true;
            ok &= check(!event.topic_truncated, "Unexpected migration topic truncation");
            ok &= check(!event.payload_truncated, "Unexpected migration payload truncation");
            break;
        }
    }
    ok &= check(saw_migration_event, "expected config.migration startup event");

    // Default-wiring path: when no supervisor is supplied, context should keep a null supervisor pointer.
    ContextFactoryOptions default_supervisor_options = options;
    default_supervisor_options.safety_supervisor     = nullptr;
    ContextWithConfig default_supervisor_out{};
    const auto default_supervisor_result =
        safety_core::system::build_context(default_supervisor_out, default_supervisor_options);
    ok &= check(default_supervisor_result.ok(), "build_context should succeed without explicit supervisor wiring");
    ok &= check(default_supervisor_out.context.safety_supervisor == nullptr,
                "context should keep null safety supervisor when options leave it unset");

    // Strict validation should fail if safety buffer is 0.
    setenv("SAFETY_CORE_CONFIG_VERSION", "2", 1);
    setenv("SAFETY_CORE_SAFETY_BUFFER_M", "0", 1);
    const std::size_t strict_events_before = transport.events.size();
    ContextWithConfig strict_invalid{};
    const auto strict_result = safety_core::system::build_context(strict_invalid, options);
    ok &= check(!strict_result.ok(), "strict validation should reject zero safety buffer");
    ok &= check(strict_result.message == "safety buffer must be > 0 in strict mode",
                "strict validation should return expected safety-buffer failure reason");
    ok &= check(strict_invalid.context.config == nullptr, "failed strict startup should not wire config pointer");
    ok &= check(strict_invalid.context.clock == nullptr, "failed strict startup should not wire clock pointer");
    ok &= check(strict_invalid.context.diagnostic_transport == nullptr,
                "failed strict startup should not wire diagnostic transport pointer");
    ok &= check(strict_invalid.context.safety_supervisor == nullptr,
                "failed strict startup should not wire safety supervisor pointer");

    bool saw_strict_warning_event = false;
    for (std::size_t i = strict_events_before; i < transport.events.size(); ++i)
    {
        if (transport.events[i].topic_view() == safety_core::diag::topic::kConfigValidationWarning)
        {
            saw_strict_warning_event = true;
            break;
        }
    }
    ok &= check(!saw_strict_warning_event,
                "strict validation failure must not publish config.validation_warning diagnostics");

    // AllowWarnings policy should accept zero safety buffer.
    ContextFactoryOptions warning_options = options;
    warning_options.validation_policy     = safety_core::config::ValidationPolicy::AllowWarnings;
    CaptureTransport warning_transport;
    warning_options.diagnostic_transport = &warning_transport;
    ContextWithConfig warning_ok{};
    const auto warning_result = safety_core::system::build_context(warning_ok, warning_options);
    ok &= check(warning_result.ok(), "warning policy should allow zero safety buffer");
    bool saw_validation_warning = false;
    for (const auto& event : warning_transport.events)
    {
        if (event.topic_view() == safety_core::diag::topic::kConfigValidationWarning)
        {
            saw_validation_warning = true;
            ok &= check(!event.topic_truncated, "Unexpected validation topic truncation");
            ok &= check(!event.payload_truncated, "Unexpected validation payload truncation");
            break;
        }
    }
    ok &= check(saw_validation_warning, "expected validation warning startup event");

    // Malformed env values should be ignored (fallback to defaults).
    setenv("SAFETY_CORE_MAX_TASKS", "6oops", 1);
    setenv("SAFETY_CORE_SAFETY_BUFFER_M", "0.2", 1);
    setenv("SAFETY_CORE_CONFIG_VERSION", "2", 1);
    ContextWithConfig malformed{};
    const auto malformed_result = safety_core::system::build_context(malformed, options);
    ok &= check(malformed_result.ok(), "malformed env input should fall back to default values");
    ok &= check(malformed.config.max_tasks == options.defaults.max_tasks,
                "malformed max_tasks should not override default");

    // Future schema version must be rejected.
    setenv("SAFETY_CORE_CONFIG_VERSION", "999", 1);
    ContextWithConfig future_invalid{};
    const auto future_result = safety_core::system::build_context(future_invalid, options);
    ok &= check(!future_result.ok(), "unsupported future config versions must be rejected");

    // Failure-path: invalid env should fail validation hook.
    setenv("SAFETY_CORE_CONFIG_VERSION", "2", 1);
    setenv("SAFETY_CORE_SAFETY_BUFFER_M", "0.2", 1);
    setenv("SAFETY_CORE_MAX_TASKS", "0", 1);
    ContextWithConfig invalid{};
    const auto invalid_result = safety_core::system::build_context(invalid, options);
    ok &= check(!invalid_result.ok(), "build_context should fail validation when max_tasks=0");

    // Schema/policy matrix: ensure startup behavior is stable across version and policy combinations.
    struct MatrixCase
    {
        const char* version;
        const char* safety_buffer;
        safety_core::config::ValidationPolicy policy;
        bool expect_ok;
        const char* expect_message;
    };

    const std::array<MatrixCase, 4U> matrix{{
        {"1", "0.2", safety_core::config::ValidationPolicy::Strict, true, ""},
        {"2", "0", safety_core::config::ValidationPolicy::AllowWarnings, true, ""},
        {"2", "0", safety_core::config::ValidationPolicy::Strict, false, "safety buffer must be > 0 in strict mode"},
        {"999", "0.2", safety_core::config::ValidationPolicy::Strict, false, "unsupported future config version"},
    }};

    for (const auto& c : matrix)
    {
        clear_env_vars();
        setenv("SAFETY_CORE_CONFIG_VERSION", c.version, 1);
        setenv("SAFETY_CORE_SAFETY_BUFFER_M", c.safety_buffer, 1);

        ContextFactoryOptions matrix_options = options;
        matrix_options.validation_policy     = c.policy;
        ContextWithConfig matrix_out{};
        const auto matrix_result = safety_core::system::build_context(matrix_out, matrix_options);

        ok &= check(matrix_result.ok() == c.expect_ok, "startup matrix expectation mismatch");
        if (!c.expect_ok)
        {
            ok &= check(matrix_result.message == c.expect_message,
                        "startup matrix failure reason should match expected message");
        }
    }

    clear_env_vars();

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] context factory tests" << '\n';
    return 0;
}
