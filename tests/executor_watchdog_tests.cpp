#include "safety_core/config/system_config.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/topics.hpp"
#include "safety_core/exec/task_executor.hpp"
#include "safety_core/platform/manual_clock.hpp"
#include "test_support.hpp"

#include <chrono>
#include <iostream>

namespace
{
    using safety_core::test_support::check;
    using CaptureTransport = safety_core::test_support::CaptureTransport;

    struct FlagTaskContext
    {
        bool* ran;
    };

    struct AdvanceTaskContext
    {
        safety_core::platform::ManualClock* clock;
    };

    struct CountTaskContext
    {
        int* run_count;
    };

    static safety_core::Result flag_task(safety_core::time::TimePoint, void* opaque) noexcept
    {
        auto* ctx = static_cast<FlagTaskContext*>(opaque);
        if (ctx != nullptr && ctx->ran != nullptr)
        {
            *(ctx->ran) = true;
        }
        return safety_core::Result::Ok();
    }

    static safety_core::Result counting_task(safety_core::time::TimePoint, void* opaque) noexcept
    {
        auto* ctx = static_cast<CountTaskContext*>(opaque);
        if (ctx != nullptr && ctx->run_count != nullptr)
        {
            ++(*ctx->run_count);
        }
        return safety_core::Result::Ok();
    }

    static safety_core::Result advancing_task(safety_core::time::TimePoint, void* opaque) noexcept
    {
        auto* ctx = static_cast<AdvanceTaskContext*>(opaque);
        if (ctx != nullptr && ctx->clock != nullptr)
        {
            ctx->clock->advance(std::chrono::milliseconds(150));
        }
        return safety_core::Result::Ok();
    }

    safety_core::config::SystemConfig make_config(std::chrono::milliseconds watchdog_period)
    {
        using namespace std::chrono_literals;

        safety_core::config::SystemConfig cfg{};
        cfg.timing.control_period           = 100ms;
        cfg.timing.watchdog_period          = watchdog_period;
        cfg.timing.localization_timeout     = 2s;
        cfg.envelope.max_speed_mps          = 1.5;
        cfg.envelope.max_accel_mps2         = 0.75;
        cfg.envelope.max_comfort_decel_mps2 = 0.8;
        cfg.envelope.control_latency_s      = 0.1;
        cfg.envelope.safety_buffer_m        = 0.2;
        cfg.max_tasks                       = 2U;
        return cfg;
    }

} // namespace

int main()
{
    using safety_core::Result;
    using safety_core::exec::CatchUpPolicy;
    using safety_core::exec::TaskExecutor;
    using safety_core::platform::ManualClock;

    bool ok = true;

    // Watchdog-path check: stale release time should trigger watchdog violation before task executes.
    {
        ManualClock clock;
        CaptureTransport transport;
        TaskExecutor<2U> executor(&clock);
        executor.set_diagnostic_transport(&transport);

        const auto cfg = make_config(std::chrono::milliseconds(120));
        ok &= check(executor.apply_config(cfg).ok(), "apply_config should succeed");

        bool ran = false;
        FlagTaskContext task_ctx{&ran};
        ok &= check(executor.add_task(flag_task, &task_ctx, safety_core::time::Duration::zero()).ok(),
                    "add_task should succeed");

        clock.advance(std::chrono::milliseconds(250));
        const Result r = executor.run_due(clock.now());
        ok &= check(r.code == safety_core::StatusCode::kDeadlineMiss, "Expected watchdog deadline miss");
        ok &= check(!ran, "Task should not execute when watchdog exceeds before release handling");
        ok &= check(!transport.events.empty(), "Expected executor diagnostics");
        if (!transport.events.empty())
        {
            ok &= check(transport.events.back().topic_view() == safety_core::diag::topic::kExecutorWatchdogExceeded,
                        "Unexpected watchdog event topic");
            ok &= check(!transport.events.back().topic_truncated, "Unexpected topic truncation");
            ok &= check(!transport.events.back().payload_truncated, "Unexpected payload truncation");
        }
    }

    // Deadline-path check: task overrun should trigger deadline miss and event.
    {
        ManualClock clock;
        CaptureTransport transport;
        TaskExecutor<2U> executor(&clock);
        executor.set_diagnostic_transport(&transport);

        const auto cfg = make_config(std::chrono::milliseconds(500));
        ok &= check(executor.apply_config(cfg).ok(), "apply_config should succeed");

        AdvanceTaskContext task_ctx{&clock};
        ok &= check(executor.add_task(advancing_task, &task_ctx, std::chrono::milliseconds(100)).ok(),
                    "add_task should succeed");

        clock.advance(std::chrono::milliseconds(100));
        const Result r = executor.run_due(clock.now());
        ok &= check(r.code == safety_core::StatusCode::kDeadlineMiss, "Expected overrun deadline miss");

        bool saw_deadline_event = false;
        for (const auto& event : transport.events)
        {
            if (event.topic_view() == safety_core::diag::topic::kExecutorDeadlineMiss)
            {
                saw_deadline_event = true;
                ok &= check(!event.topic_truncated, "Unexpected topic truncation");
                ok &= check(!event.payload_truncated, "Unexpected payload truncation");
                break;
            }
        }
        ok &= check(saw_deadline_event, "Expected executor.deadline_miss diagnostic event");
    }

    // Single-step catch-up: only one backlog release should run per tick.
    {
        ManualClock clock;
        CaptureTransport transport;
        TaskExecutor<2U> executor(&clock);
        executor.set_diagnostic_transport(&transport);

        const auto cfg = make_config(std::chrono::milliseconds(0));
        ok &= check(executor.apply_config(cfg).ok(), "apply_config should succeed");

        int run_count = 0;
        CountTaskContext task_ctx{&run_count};
        ok &= check(executor.add_task(counting_task, &task_ctx, std::chrono::milliseconds(100)).ok(),
                    "add_task should succeed");

        const auto simulated_now = clock.now() + std::chrono::milliseconds(450);
        ok &= check(executor.run_due(simulated_now).ok(), "run_due should succeed in single-step mode");
        ok &= check(run_count == 1, "single-step mode should execute only one due release per tick");
    }

    // Bounded catch-up: should execute up to configured limit and emit catch-up-limited diagnostic.
    {
        ManualClock clock;
        CaptureTransport transport;
        TaskExecutor<2U> executor(&clock);
        executor.set_diagnostic_transport(&transport);
        executor.set_catch_up_policy(CatchUpPolicy::BoundedCatchUp, 2U);

        const auto cfg = make_config(std::chrono::milliseconds(0));
        ok &= check(executor.apply_config(cfg).ok(), "apply_config should succeed");

        int run_count = 0;
        CountTaskContext task_ctx{&run_count};
        ok &= check(executor.add_task(counting_task, &task_ctx, std::chrono::milliseconds(100)).ok(),
                    "add_task should succeed");

        const auto simulated_now = clock.now() + std::chrono::milliseconds(450);
        ok &= check(executor.run_due(simulated_now).ok(), "run_due should succeed in bounded catch-up mode");
        ok &= check(run_count == 2, "bounded catch-up should execute up to configured limit");

        bool saw_limited_event = false;
        for (const auto& event : transport.events)
        {
            if (event.topic_view() == safety_core::diag::topic::kExecutorCatchUpLimited)
            {
                saw_limited_event = true;
                break;
            }
        }
        ok &= check(saw_limited_event, "expected executor.catch_up_limited diagnostic event");
    }

    // Add-task overflow guard: release timestamp computation must fail safely near timepoint max.
    {
        ManualClock clock;
        CaptureTransport transport;
        TaskExecutor<2U> executor(&clock);
        executor.set_diagnostic_transport(&transport);

        const auto cfg = make_config(std::chrono::milliseconds(0));
        ok &= check(executor.apply_config(cfg).ok(), "apply_config should succeed");

        clock.set(safety_core::time::TimePoint::max() - std::chrono::nanoseconds(50));
        int run_count = 0;
        CountTaskContext task_ctx{&run_count};
        const Result r = executor.add_task(counting_task, &task_ctx, std::chrono::nanoseconds(100));
        ok &= check(r.code == safety_core::StatusCode::kFault, "add_task should fault on release time overflow");

        bool saw_error_event = false;
        for (const auto& event : transport.events)
        {
            if (event.topic_view() == safety_core::diag::topic::kExecutorTaskError)
            {
                saw_error_event = true;
                break;
            }
        }
        ok &= check(saw_error_event, "expected executor.task_error overflow diagnostic");
    }

    // Run-due overflow guard: deadline/next-release progression must fault without wrapping.
    {
        ManualClock clock;
        CaptureTransport transport;
        TaskExecutor<2U> executor(&clock);
        executor.set_diagnostic_transport(&transport);

        const auto cfg = make_config(std::chrono::milliseconds(0));
        ok &= check(executor.apply_config(cfg).ok(), "apply_config should succeed");

        clock.set(safety_core::time::TimePoint::max() - std::chrono::nanoseconds(150));
        int run_count = 0;
        CountTaskContext task_ctx{&run_count};
        ok &= check(executor.add_task(counting_task, &task_ctx, std::chrono::nanoseconds(100)).ok(),
                    "add_task should succeed before overflow boundary");

        const Result r = executor.run_due(safety_core::time::TimePoint::max());
        ok &= check(r.code == safety_core::StatusCode::kFault,
                    "run_due should fault when deadline computation would overflow");
        ok &= check(run_count == 0, "task should not run when deadline overflow is detected pre-dispatch");

        bool saw_error_event = false;
        for (const auto& event : transport.events)
        {
            if (event.topic_view() == safety_core::diag::topic::kExecutorTaskError)
            {
                saw_error_event = true;
                break;
            }
        }
        ok &= check(saw_error_event, "expected overflow task_error diagnostic during run_due");
    }

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] executor watchdog tests" << '\n';
    return 0;
}
