#include "safety_core/config/system_config.hpp"
#include "safety_core/control/safety_pid.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/topics.hpp"
#include "safety_core/platform/manual_clock.hpp"

#include <cmath>
#include <iostream>
#include <vector>

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

    class CaptureTransport final : public safety_core::diag::DiagnosticTransport
    {
      public:
        void publish(const safety_core::diag::DiagnosticEvent& event) noexcept override
        {
            events.push_back(event);
        }

        std::vector<safety_core::diag::DiagnosticEvent> events{};
    };

    safety_core::config::SystemConfig make_config()
    {
        using namespace std::chrono_literals;
        safety_core::config::SystemConfig cfg{};
        cfg.timing.control_period           = 100ms;
        cfg.timing.watchdog_period          = 150ms;
        cfg.timing.localization_timeout     = 500ms;
        cfg.envelope.max_speed_mps          = 1.5;
        cfg.envelope.max_accel_mps2         = 0.75;
        cfg.envelope.max_comfort_decel_mps2 = 0.8;
        cfg.envelope.control_latency_s      = 0.1;
        cfg.envelope.safety_buffer_m        = 0.2;
        cfg.max_tasks                       = 4U;
        return cfg;
    }

} // namespace

int main()
{
    using safety_core::config::SystemConfig;
    using safety_core::control::PidGains;
    using safety_core::control::SafetyPidController;
    using safety_core::platform::ManualClock;
    using safety_core::time::Duration;
    using safety_core::time::TimePoint;

    SystemConfig cfg = make_config();
    ManualClock clock;
    clock.set(TimePoint{});

    SafetyPidController pid(&clock);
    CaptureTransport transport;
    pid.set_diagnostic_transport(&transport);
    bool ok = true;
    ok &= pid.apply_config(cfg).ok();
    pid.set_gains(PidGains{1.0, 0.2, 0.05});

    // Force localization timeout by marking stale update.
    const auto stale_stamp = clock.now() - cfg.timing.localization_timeout - Duration(1);
    pid.mark_localization_update(stale_stamp);
    double output = pid.compute(1.0, 0.0);
    ok &= check(std::abs(output) < 1e-9, "Output should be zero when localization is stale");
    ok &= check(!transport.events.empty(), "Expected stale-localization diagnostic event");
    if (!transport.events.empty())
    {
        ok &= check(transport.events.back().topic_view() == safety_core::diag::topic::kPidLocalizationStale,
                    "Wrong stale-localization topic");
        ok &= check(!transport.events.back().topic_truncated, "Unexpected topic truncation");
        ok &= check(!transport.events.back().payload_truncated, "Unexpected payload truncation");
    }

    // Fresh localization update should allow commands within speed limits.
    pid.mark_localization_update(clock.now());
    output = pid.compute(0.8, 0.0);
    ok &= check(std::abs(output) <= cfg.envelope.max_speed_mps + 1e-6, "Output should respect max speed");
    ok &= check(!transport.events.empty(), "Expected PID output diagnostics");

    // Advance time and command a large change to exercise acceleration clamp.
    clock.advance(cfg.timing.control_period);
    pid.mark_localization_update(clock.now());
    const double prev_output = pid.compute(0.5, 0.0);
    clock.advance(cfg.timing.control_period);
    pid.mark_localization_update(clock.now());
    const double surge = pid.compute(5.0, 0.0);
    const double accel_limit =
        cfg.envelope.max_accel_mps2 * std::chrono::duration<double>(cfg.timing.control_period).count();
    ok &= check(std::abs(surge - prev_output) <= accel_limit + 1e-6, "Acceleration clamp should hold");
    if (!transport.events.empty())
    {
        ok &= check(transport.events.back().topic_view() == safety_core::diag::topic::kPidOutput,
                    "Expected pid.output event as latest diagnostic");
        ok &= check(!transport.events.back().topic_truncated, "Unexpected topic truncation");
        ok &= check(!transport.events.back().payload_truncated, "Unexpected payload truncation");
    }

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] pid controller tests" << '\n';
    return 0;
}
