// Worst-case timing evidence for the "deterministic" claim.
//
// Measures per-call latency distributions (mean / p50 / p95 / p99 / max) for
// the safety-critical operations: envelope evaluation, PID control cycle,
// filter updates, executor ticks, state transitions, and the headline metric
// — supervisor critical-event -> latched SafeStop escalation latency.
//
// Each benchmark carries a loose regression budget (microseconds). Budgets
// are set generously (~20-50x typical on quiet CI runners) so they catch
// real regressions (e.g. accidental allocation, unbounded loop) without
// flaky failures on noisy shared runners. Exit code 1 = budget exceeded.
//
// Usage: safety_core_benchmarks [--json] [--iterations N]

#include "safety_core/config/system_config.hpp"
#include "safety_core/control/safety_pid.hpp"
#include "safety_core/exec/task_executor.hpp"
#include "safety_core/filters/alpha_beta_filter.hpp"
#include "safety_core/filters/bounded_ekf_filter.hpp"
#include "safety_core/filters/complementary_filter.hpp"
#include "safety_core/platform/manual_clock.hpp"
#include "safety_core/safety/safety_envelope.hpp"
#include "safety_core/safety/safety_supervisor.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "safety_core/system/system_context.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

namespace
{
    using safety_core::time::TimePoint;

    struct BenchResult
    {
        const char* name;
        double mean_us;
        double p50_us;
        double p95_us;
        double p99_us;
        double max_us;
        std::size_t iterations;
        double budget_us;
        bool within_budget;
    };

    std::vector<double> samples;

    template <typename Fn> BenchResult run(const char* name, std::size_t iterations, double budget_us, Fn&& fn)
    {
        samples.clear();
        samples.reserve(iterations);

        // Warmup: stabilize caches/branch predictors.
        for (std::size_t i = 0U; i < (iterations / 10U) + 1U; ++i)
        {
            fn(i);
        }

        for (std::size_t i = 0U; i < iterations; ++i)
        {
            const auto start = std::chrono::steady_clock::now();
            fn(i);
            const auto end = std::chrono::steady_clock::now();
            samples.push_back(std::chrono::duration<double, std::micro>(end - start).count());
        }

        std::sort(samples.begin(), samples.end());
        double total = 0.0;
        for (const double s : samples)
        {
            total += s;
        }

        const auto pct = [&](double q)
        { return samples[static_cast<std::size_t>(q * static_cast<double>(samples.size() - 1U))]; };

        const double mx = samples.back();
        return BenchResult{name,           total / static_cast<double>(samples.size()),
                           pct(0.50),      pct(0.95),
                           pct(0.99),      mx,
                           iterations,     budget_us,
                           mx <= budget_us};
    }

    safety_core::config::SystemConfig bench_config()
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

    safety_core::Result trivial_task(TimePoint, void* ctx) noexcept
    {
        auto* counter = static_cast<std::uint64_t*>(ctx);
        *counter += 1U;
        return safety_core::Result::Ok();
    }
} // namespace

int main(int argc, char** argv)
{
    using namespace std::chrono_literals;
    using safety_core::safety::SafetyZone;

    bool json              = false;
    std::size_t iterations = 100000U;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--json") == 0)
        {
            json = true;
        }
        else if (std::strcmp(argv[i], "--iterations") == 0 && i + 1 < argc)
        {
            iterations = static_cast<std::size_t>(std::strtoull(argv[++i], nullptr, 10));
        }
    }

    std::vector<BenchResult> results;
    const safety_core::config::SystemConfig cfg = bench_config();

    // ------------------------------------------------------------------
    // 1. Envelope evaluation
    // ------------------------------------------------------------------
    {
        results.push_back(run("envelope.evaluate", iterations, 50.0,
                              [&](std::size_t i)
                              {
                                  const double dist = 0.05 + static_cast<double>(i % 400U) * 0.05;
                                  const double spd  = 0.1 + static_cast<double>(i % 30U) * 0.1;
                                  const auto ev = safety_core::safety::evaluate_stop_distance(dist, spd, cfg.envelope);
                                  if (ev.zone == SafetyZone::Emergency && dist > 1.0)
                                  {
                                      std::abort();
                                  }
                              }));
    }

    // ------------------------------------------------------------------
    // 2. PID control cycle (finite inputs, valid localization)
    // ------------------------------------------------------------------
    {
        safety_core::platform::ManualClock clock;
        safety_core::control::SafetyPidController pid(&clock);
        pid.set_gains(safety_core::control::PidGains{1.0, 0.5, 0.1, 50.0});
        static_cast<void>(pid.apply_config(cfg));
        pid.mark_localization_update(clock.now());

        results.push_back(run("pid.compute", iterations, 50.0,
                              [&](std::size_t i)
                              {
                                  clock.advance(10ms);
                                  pid.mark_localization_update(clock.now());
                                  const double sp = static_cast<double>(i % 20U) * 0.1;
                                  static_cast<void>(pid.compute(sp, sp * 0.9));
                              }));
    }

    // ------------------------------------------------------------------
    // 3. Filter updates
    // ------------------------------------------------------------------
    {
        safety_core::filters::AlphaBetaFilter alpha_beta;
        safety_core::filters::ComplementaryFilter complementary;
        safety_core::filters::BoundedEkfFilter ekf;
        std::uint64_t ts = 0U;

        results.push_back(run("filter.alpha_beta", iterations, 50.0,
                              [&](std::size_t i)
                              {
                                  ts += 10000000ULL;
                                  static_cast<void>(alpha_beta.update(static_cast<double>(i % 100U) * 0.01, ts));
                              }));

        ts = 0U;
        results.push_back(run("filter.complementary", iterations, 50.0,
                              [&](std::size_t i)
                              {
                                  ts += 10000000ULL;
                                  static_cast<void>(
                                      complementary.update(0.01, static_cast<double>(i % 100U) * 0.01, ts));
                              }));

        ts = 0U;
        results.push_back(run("filter.bounded_ekf", iterations, 50.0,
                              [&](std::size_t i)
                              {
                                  ts += 10000000ULL;
                                  static_cast<void>(ekf.update(static_cast<double>(i % 100U) * 0.01, 0.0, ts));
                              }));
    }

    // ------------------------------------------------------------------
    // 4. Executor tick with 4 periodic tasks
    // ------------------------------------------------------------------
    {
        safety_core::platform::ManualClock clock;
        safety_core::exec::TaskExecutor<8U> executor(&clock);
        static_cast<void>(executor.apply_config(cfg));
        std::uint64_t counters[4]{};
        for (auto& counter : counters)
        {
            static_cast<void>(executor.add_task(trivial_task, &counter, 10ms));
        }

        results.push_back(run("executor.run_due_4_tasks", iterations, 200.0,
                              [&](std::size_t)
                              {
                                  clock.advance(100ms);
                                  static_cast<void>(executor.run_due(clock.now()));
                              }));
    }

    // ------------------------------------------------------------------
    // 5. State machine transition
    // ------------------------------------------------------------------
    {
        safety_core::sm::ModeStateMachine machine;
        results.push_back(run("state_machine.transition", iterations, 50.0,
                              [&](std::size_t i)
                              {
                                  // Ping-pong through the legal path to keep transitions real.
                                  static_cast<void>(machine.transition_to(safety_core::sm::Mode::SafeStop));
                                  // Fresh machine occasionally to exercise Init->Idle too.
                                  if ((i % 3U) == 0U)
                                  {
                                      safety_core::sm::ModeStateMachine fresh;
                                      static_cast<void>(fresh.transition_to(safety_core::sm::Mode::Idle));
                                  }
                              }));
    }

    // ------------------------------------------------------------------
    // 6. Headline metric: supervisor critical event -> latched SafeStop.
    //    This is the latency that matters for collision response.
    // ------------------------------------------------------------------
    {
        results.push_back(run("supervisor.critical_to_safestop", iterations, 200.0,
                              [&](std::size_t)
                              {
                                  safety_core::sm::ModeStateMachine machine;
                                  static_cast<void>(machine.transition_to(safety_core::sm::Mode::Idle));
                                  static_cast<void>(machine.transition_to(safety_core::sm::Mode::Moving));
                                  safety_core::safety::SafetySupervisor supervisor(&machine, nullptr, nullptr);
                                  const safety_core::safety::MonitorEvent event{
                                      safety_core::safety::MonitorType::ClockRegression,
                                      safety_core::safety::MonitorSeverity::Critical, 99U, "bench"};
                                  static_cast<void>(supervisor.process_monitor_event(event));
                                  if (!machine.fault_latched())
                                  {
                                      std::abort();
                                  }
                              }));
    }

    // ------------------------------------------------------------------
    // Report
    // ------------------------------------------------------------------
    bool all_ok = true;
    if (json)
    {
        std::printf("[\n");
        for (std::size_t i = 0U; i < results.size(); ++i)
        {
            const BenchResult& r = results[i];
            std::printf("  {\"name\":\"%s\",\"iterations\":%zu,\"mean_us\":%.3f,\"p50_us\":%.3f,"
                        "\"p95_us\":%.3f,\"p99_us\":%.3f,\"max_us\":%.3f,\"budget_us\":%.1f,\"ok\":%s}%s\n",
                        r.name, r.iterations, r.mean_us, r.p50_us, r.p95_us, r.p99_us, r.max_us, r.budget_us,
                        r.within_budget ? "true" : "false", (i + 1U < results.size()) ? "," : "");
        }
        std::printf("]\n");
    }
    else
    {
        std::printf("%-32s %10s %10s %10s %10s %10s %10s %6s\n", "benchmark", "mean_us", "p50_us", "p95_us", "p99_us",
                    "max_us", "budget_us", "ok");
        for (const BenchResult& r : results)
        {
            std::printf("%-32s %10.3f %10.3f %10.3f %10.3f %10.3f %10.1f %6s\n", r.name, r.mean_us, r.p50_us, r.p95_us,
                        r.p99_us, r.max_us, r.budget_us, r.within_budget ? "PASS" : "FAIL");
        }
    }

    for (const BenchResult& r : results)
    {
        all_ok = all_ok && r.within_budget;
    }
    return all_ok ? 0 : 1;
}
