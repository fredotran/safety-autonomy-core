// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/config/env_loader.hpp"
#include "safety_core/diag/logging_diagnostic_transport.hpp"
#include "safety_core/diag/logging_health_monitor.hpp"
#include "safety_core/exec/task_executor.hpp"
#include "safety_core/platform/clock.hpp"
#include "safety_core/result.hpp"
#include "safety_core/safety/safety_supervisor.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "safety_core/system/context_factory.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace
{

    safety_core::config::SystemConfig make_defaults()
    {
        using namespace std::chrono_literals;

        safety_core::config::SystemConfig cfg{};
        cfg.config_version                  = safety_core::config::kCurrentSystemConfigVersion;
        cfg.timing.control_period           = 20ms;
        cfg.timing.watchdog_period          = 40ms;
        cfg.timing.localization_timeout     = 2s;
        cfg.envelope.max_speed_mps          = 1.5;
        cfg.envelope.max_accel_mps2         = 0.75;
        cfg.envelope.max_comfort_decel_mps2 = 0.8;
        cfg.envelope.control_latency_s      = 0.1;
        cfg.envelope.safety_buffer_m        = 0.2;
        cfg.max_tasks                       = 4U;
        return cfg;
    }

    void print_config_summary(const safety_core::config::SystemConfig& cfg)
    {
        std::cout << "[safety_core_app] ----- System Config -----\n";
        std::cout << "  control_period_ns=" << cfg.timing.control_period.count() << '\n';
        std::cout << "  watchdog_period_ns=" << cfg.timing.watchdog_period.count() << '\n';
        std::cout << "  localization_timeout_ns=" << cfg.timing.localization_timeout.count() << '\n';
        std::cout << "  envelope.max_speed_mps=" << cfg.envelope.max_speed_mps << '\n';
        std::cout << "  envelope.max_accel_mps2=" << cfg.envelope.max_accel_mps2 << '\n';
        std::cout << "  envelope.max_comfort_decel_mps2=" << cfg.envelope.max_comfort_decel_mps2 << '\n';
        std::cout << "  envelope.control_latency_s=" << cfg.envelope.control_latency_s << '\n';
        std::cout << "  envelope.safety_buffer_m=" << cfg.envelope.safety_buffer_m << '\n';
        std::cout << "  max_tasks=" << static_cast<unsigned>(cfg.max_tasks) << '\n';
        std::cout << "[safety_core_app] -------------------------\n";
    }

    struct HeartbeatContext
    {
        int cycles{0};
    };

    safety_core::Result heartbeat_task(safety_core::time::TimePoint, void* opaque) noexcept
    {
        auto* ctx = static_cast<HeartbeatContext*>(opaque);
        if (ctx != nullptr)
        {
            ++ctx->cycles;
            std::cout << "[safety_core_app] heartbeat cycle=" << ctx->cycles << '\n';
        }
        return safety_core::Result::Ok();
    }

    struct LocalizationMonitorContext
    {
        safety_core::safety::SafetySupervisor* supervisor{nullptr};
        std::uint64_t localization_age_ns{0U};
    };

    safety_core::Result localization_monitor_task(safety_core::time::TimePoint, void* opaque) noexcept
    {
        auto* ctx = static_cast<LocalizationMonitorContext*>(opaque);
        if ((ctx == nullptr) || (ctx->supervisor == nullptr))
        {
            return safety_core::Result::InvalidState("monitor context missing");
        }

        ctx->localization_age_ns += 10'000'000ULL; // 10 ms increments
        const auto res = ctx->supervisor->observe_localization_age(ctx->localization_age_ns, 80'000'000ULL);
        if (!res.ok())
        {
            std::cerr << "[safety_core_app] localization age monitor returned error: " << res.message << '\n';
        }

        if (ctx->localization_age_ns > 120'000'000ULL)
        {
            ctx->localization_age_ns = 0U;
        }
        return safety_core::Result::Ok();
    }

} // namespace

int main()
{
    using safety_core::config::SystemConfig;
    using safety_core::diag::LoggingDiagnosticTransport;
    using safety_core::diag::LoggingHealthMonitor;
    using safety_core::exec::TaskExecutor;
    using safety_core::platform::SteadyClock;
    using safety_core::system::ContextFactoryOptions;
    using safety_core::system::ContextWithConfig;

    SystemConfig defaults         = make_defaults();
    const SystemConfig configured = safety_core::config::load_from_env(defaults);

    LoggingDiagnosticTransport diag_transport(std::cout);
    LoggingHealthMonitor health_monitor(std::cout, SteadyClock::instance_ptr());
    safety_core::sm::ModeStateMachine state_machine;
    safety_core::safety::SafetySupervisor supervisor(&state_machine, &diag_transport, SteadyClock::instance_ptr());

    ContextFactoryOptions options{};
    options.defaults             = configured;
    options.clock                = SteadyClock::instance_ptr();
    options.health_monitor       = &health_monitor;
    options.diagnostic_transport = &diag_transport;
    options.safety_supervisor    = &supervisor;

    ContextWithConfig context{};
    const auto result = safety_core::system::build_context(context, options);
    if (!result.ok())
    {
        std::cerr << "[safety_core_app] context build failed: " << result.message << '\n';
        return 1;
    }

    print_config_summary(context.config);
    std::cout << "[safety_core_app] initial mode=" << static_cast<int>(state_machine.mode()) << '\n';

    TaskExecutor<> executor(options.clock);
    auto exec_result = executor.apply_config(context.config);
    if (!exec_result.ok())
    {
        std::cerr << "[safety_core_app] executor apply_config failed: " << exec_result.message << '\n';
        return 1;
    }
    executor.set_diagnostic_transport(&diag_transport);

    HeartbeatContext heartbeat{};
    exec_result = executor.add_task(&heartbeat_task, &heartbeat, context.config.timing.control_period);
    if (!exec_result.ok())
    {
        std::cerr << "[safety_core_app] failed to add heartbeat task: " << exec_result.message << '\n';
        return 1;
    }

    LocalizationMonitorContext monitor_ctx{&supervisor, 0U};
    exec_result = executor.add_task(&localization_monitor_task, &monitor_ctx, context.config.timing.control_period * 2);
    if (!exec_result.ok())
    {
        std::cerr << "[safety_core_app] failed to add localization monitor task: " << exec_result.message << '\n';
        return 1;
    }

    std::cout << "[safety_core_app] running demo loop (10 iterations)." << '\n';

    for (int iteration = 0; iteration < 10; ++iteration)
    {
        const auto now                       = safety_core::time::now();
        const safety_core::Result run_result = executor.run_due(now);
        if (!run_result.ok())
        {
            std::cerr << "[safety_core_app] executor run_due failed: " << run_result.message << '\n';
            return 1;
        }

        if (iteration == 5)
        {
            safety_core::safety::MonitorEvent degraded_event{safety_core::safety::MonitorType::LocalizationStale,
                                                             safety_core::safety::MonitorSeverity::Degraded, 42U,
                                                             "demo-induced stale localization"};
            const auto sup_res = supervisor.process_monitor_event(degraded_event);
            std::cout << "[safety_core_app] injected degraded monitor event (result=" << sup_res.message << ")\n";
        }

        std::this_thread::sleep_for(context.config.timing.control_period);
    }

    std::cout << "[safety_core_app] demo complete. Final mode=" << static_cast<int>(state_machine.mode()) << '\n';
    return 0;
}
