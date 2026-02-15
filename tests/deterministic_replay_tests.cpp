#include "safety_core/config/system_config.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/topics.hpp"
#include "safety_core/exec/task_executor.hpp"
#include "safety_core/platform/manual_clock.hpp"

#include <chrono>
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

    struct CountTaskContext
    {
        int* run_count;
    };

    static safety_core::Result counting_task(safety_core::time::TimePoint, void* opaque) noexcept
    {
        auto* ctx = static_cast<CountTaskContext*>(opaque);
        if (ctx != nullptr && ctx->run_count != nullptr)
        {
            ++(*ctx->run_count);
        }
        return safety_core::Result::Ok();
    }

    safety_core::config::SystemConfig make_config()
    {
        using namespace std::chrono_literals;

        safety_core::config::SystemConfig cfg{};
        cfg.config_version                  = safety_core::config::kCurrentSystemConfigVersion;
        cfg.timing.control_period           = 100ms;
        cfg.timing.watchdog_period          = 0ms;
        cfg.timing.localization_timeout     = 2s;
        cfg.envelope.max_speed_mps          = 1.2;
        cfg.envelope.max_accel_mps2         = 0.6;
        cfg.envelope.max_comfort_decel_mps2 = 0.8;
        cfg.envelope.control_latency_s      = 0.1;
        cfg.envelope.safety_buffer_m        = 0.2;
        cfg.max_tasks                       = 2U;
        return cfg;
    }

    std::vector<safety_core::diag::DiagnosticEvent> run_scenario()
    {
        using safety_core::exec::CatchUpPolicy;
        using safety_core::exec::TaskExecutor;

        safety_core::platform::ManualClock clock;
        CaptureTransport transport;

        TaskExecutor<2U> executor(&clock);
        executor.set_diagnostic_transport(&transport);
        executor.set_catch_up_policy(CatchUpPolicy::BoundedCatchUp, 2U);
        static_cast<void>(executor.apply_config(make_config()));

        int run_count = 0;
        CountTaskContext task_ctx{&run_count};
        static_cast<void>(executor.add_task(counting_task, &task_ctx, std::chrono::milliseconds(100)));

        const auto simulated_now = clock.now() + std::chrono::milliseconds(450);
        static_cast<void>(executor.run_due(simulated_now));

        return transport.events;
    }

    bool events_equal(const safety_core::diag::DiagnosticEvent& a, const safety_core::diag::DiagnosticEvent& b)
    {
        return (a.topic_view() == b.topic_view()) && (a.payload_view() == b.payload_view()) &&
               (a.timestamp_ns == b.timestamp_ns) && (a.topic_truncated == b.topic_truncated) &&
               (a.payload_truncated == b.payload_truncated);
    }

} // namespace

int main()
{
    bool ok = true;

    const auto first  = run_scenario();
    const auto second = run_scenario();

    ok &= check(first.size() == second.size(), "deterministic replay should produce same event count");
    for (std::size_t i = 0U; (i < first.size()) && (i < second.size()); ++i)
    {
        ok &= check(events_equal(first[i], second[i]), "deterministic replay mismatch in event payload");
    }

    ok &= check(first.size() == 3U, "expected deterministic bounded-catchup event sequence size");
    if (first.size() == 3U)
    {
        ok &= check(first[0].topic_view() == safety_core::diag::topic::kExecutorTaskAdded,
                    "first event should be task_added");
        ok &= check(first[1].topic_view() == safety_core::diag::topic::kExecutorCatchUpLimited,
                    "second event should be catch_up_limited");
        ok &= check(first[2].topic_view() == safety_core::diag::topic::kExecutorTaskRun,
                    "third event should be task_run");
    }

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] deterministic replay tests" << '\n';
    return 0;
}
