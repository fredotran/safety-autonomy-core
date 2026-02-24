#include "safety_core/config/system_config.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/exec/task_executor.hpp"
#include "safety_core/filters/bounded_ekf_filter.hpp"
#include "safety_core/platform/manual_clock.hpp"
#include "test_support.hpp"

#include <chrono>
#include <cmath>
#include <iostream>

namespace
{
    using safety_core::test_support::check;
    using FaultClock = safety_core::platform::ManualClock;

    using CaptureTransport = safety_core::test_support::CaptureTransport;

    class DropTransport final : public safety_core::diag::DiagnosticTransport
    {
      public:
        void publish(const safety_core::diag::DiagnosticEvent&) noexcept override
        {
            ++drop_count;
        }

        std::size_t drop_count{0U};
    };

    struct CountTaskContext
    {
        int* count;
    };

    safety_core::Result count_task(safety_core::time::TimePoint, void* opaque) noexcept
    {
        auto* ctx = static_cast<CountTaskContext*>(opaque);
        if ((ctx != nullptr) && (ctx->count != nullptr))
        {
            ++(*ctx->count);
        }
        return safety_core::Result::Ok();
    }

    safety_core::config::SystemConfig make_config()
    {
        using namespace std::chrono_literals;

        safety_core::config::SystemConfig cfg{};
        cfg.config_version                  = safety_core::config::kCurrentSystemConfigVersion;
        cfg.timing.control_period           = 100ms;
        cfg.timing.watchdog_period          = 200ms;
        cfg.timing.localization_timeout     = 2s;
        cfg.envelope.max_speed_mps          = 2.0;
        cfg.envelope.max_accel_mps2         = 1.0;
        cfg.envelope.max_comfort_decel_mps2 = 1.0;
        cfg.envelope.control_latency_s      = 0.1;
        cfg.envelope.safety_buffer_m        = 0.2;
        cfg.max_tasks                       = 2U;
        return cfg;
    }

} // namespace

int main()
{
    using safety_core::exec::TaskExecutor;

    bool ok = true;

    // Fault injection: backward time jump should not crash and should preserve deterministic run accounting.
    {
        FaultClock clock;
        CaptureTransport transport;
        TaskExecutor<2U> executor(&clock);
        executor.set_diagnostic_transport(&transport);
        ok &= check(executor.apply_config(make_config()).ok(), "apply_config should succeed");

        int run_count = 0;
        CountTaskContext task_ctx{&run_count};
        ok &= check(executor.add_task(count_task, &task_ctx, std::chrono::milliseconds(100)).ok(),
                    "add_task should succeed");

        clock.advance(std::chrono::milliseconds(100));
        ok &= check(executor.run_due(clock.now()).ok(), "first due run should succeed");
        ok &= check(run_count == 1, "task should execute once after first period");

        clock.set(clock.now() - std::chrono::milliseconds(80));
        ok &= check(executor.run_due(clock.now()).ok(), "backward time run_due should not fail");
        ok &= check(run_count == 1, "backward jump should not spuriously rerun task");
    }

    // Fault injection: dropping transport should not affect scheduler correctness.
    {
        FaultClock clock;
        DropTransport drop_transport;
        TaskExecutor<2U> executor(&clock);
        executor.set_diagnostic_transport(&drop_transport);
        ok &= check(executor.apply_config(make_config()).ok(), "apply_config should succeed");

        int run_count = 0;
        CountTaskContext task_ctx{&run_count};
        ok &= check(executor.add_task(count_task, &task_ctx, std::chrono::milliseconds(100)).ok(),
                    "add_task should succeed with drop transport");

        clock.advance(std::chrono::milliseconds(100));
        ok &= check(executor.run_due(clock.now()).ok(), "run_due should succeed with drop transport");
        ok &= check(run_count == 1, "task execution should be independent of transport reliability");
        ok &= check(drop_transport.drop_count > 0U, "drop transport should have observed publish attempts");
    }

    // Fault injection: sensor NaN burst should be rejected by bounded EKF and mark unhealthy.
    {
        safety_core::filters::BoundedEkfFilter ekf;
        const auto cfg = make_config();
        ekf.apply_config(cfg);
        ekf.reset(0.0, 0.0);

        ok &= check(ekf.update(1.0, 0.1), "finite EKF update should succeed");
        ok &= check(!ekf.update(NAN, 0.1), "NaN measurement should be rejected");
        ok &= check(!ekf.healthy(), "NaN burst should mark EKF unhealthy");
    }

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] fault injection tests" << '\n';
    return 0;
}
