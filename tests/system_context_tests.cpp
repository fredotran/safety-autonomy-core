#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/health_beacon_publisher.hpp"
#include "safety_core/diag/logging_health_monitor.hpp"
#include "safety_core/diag/topics.hpp"
#include "safety_core/exec/task_executor.hpp"
#include "safety_core/platform/clock.hpp"
#include "safety_core/safety/safety_envelope.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "safety_core/system/system_context.hpp"
#include "test_support.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string_view>

namespace
{
    using safety_core::test_support::check;

    class StubClock : public safety_core::platform::Clock
    {
      public:
        StubClock()
        {
            now_ = safety_core::time::TimePoint{};
        }

        [[nodiscard]] safety_core::time::TimePoint now() const noexcept override
        {
            return now_;
        }

        void advance(safety_core::time::Duration delta) noexcept
        {
            now_ += delta;
        }

      private:
        safety_core::time::TimePoint now_;
    };

    using CaptureTransport = safety_core::test_support::CaptureTransport;

    struct FlagTaskContext
    {
        bool* ran;
    };

    safety_core::Result mark_ran_task(safety_core::time::TimePoint, void* opaque) noexcept
    {
        auto* ctx = static_cast<FlagTaskContext*>(opaque);
        if (ctx != nullptr && ctx->ran != nullptr)
        {
            *(ctx->ran) = true;
        }
        return safety_core::Result::Ok();
    }

} // namespace

int main()
{
    using safety_core::config::MotionEnvelopeConfig;
    using safety_core::config::SystemConfig;
    using safety_core::config::TimingConfig;
    using safety_core::diag::HealthBeaconPublisher;
    using safety_core::diag::LoggingHealthMonitor;
    using safety_core::exec::TaskExecutor;
    using safety_core::safety::evaluate_stop_distance;
    using safety_core::sm::Mode;
    using safety_core::sm::ModeStateMachine;
    using safety_core::system::SystemContext;

    const SystemConfig config{
        TimingConfig{std::chrono::milliseconds(100), std::chrono::milliseconds(200), std::chrono::seconds(2)},
        MotionEnvelopeConfig{1.5, 0.5, 0.8, 0.1, 0.2}, 2U};

    bool ok = true;

    // Verify safety envelope convenience overload works with MotionEnvelopeConfig.
    const auto envelope_eval = evaluate_stop_distance(5.0, 1.0, config.envelope);
    ok &= check(envelope_eval.within_envelope, "Envelope evaluation should pass for ample clearance");

    // Logging monitor + diagnostic transport integration via SystemContext constructor.
    StubClock monitor_clock;
    std::ostringstream log_sink;
    LoggingHealthMonitor monitor(log_sink, &monitor_clock);
    CaptureTransport transport;
    SystemContext context{&config, &monitor_clock, &monitor, &transport};

    ModeStateMachine machine(context);
    ok &= check(machine.transition_to(Mode::Idle).ok(), "Init -> Idle via context");
    machine.latch_fault(7U);
    const std::string logs = log_sink.str();
    ok &= check(logs.find("ts=") != std::string::npos, "Monitor should emit timestamps");
    ok &= check(logs.find("mode_transition") != std::string::npos, "Monitor should log transitions");
    ok &= check(logs.find("fault_latched") != std::string::npos, "Monitor should log faults");

    // TaskExecutor wiring with context clock + diagnostic transport should emit watchdog topic.
    StubClock stub_clock;
    TaskExecutor<2U> executor(&stub_clock);
    executor.set_diagnostic_transport(&transport);
    ok &= check(executor.apply_config(config).ok(), "Executor accepts config");
    bool ran = false;
    FlagTaskContext task_ctx{&ran};
    ok &= check(executor.add_task(mark_ran_task, &task_ctx, safety_core::time::Duration::zero()).ok(),
                "Task add should succeed");

    stub_clock.advance(std::chrono::milliseconds(350));
    const auto run_result = executor.run_due(stub_clock.now());
    ok &= check(run_result.code == safety_core::StatusCode::kDeadlineMiss,
                "Executor should report watchdog deadline miss under excessive lag");
    ok &= check(!ran, "Task should not execute once watchdog violation is detected");

    bool saw_watchdog_event = false;
    for (const auto& event : transport.events)
    {
        if (event.topic_view() == safety_core::diag::topic::kExecutorWatchdogExceeded)
        {
            saw_watchdog_event = true;
            break;
        }
    }
    ok &= check(saw_watchdog_event, "Expected executor.watchdog_exceeded diagnostic event");

    // Health beacon should publish on shared transport and include fault/watchdog context.
    HealthBeaconPublisher beacon(&transport);
    beacon.set_period(std::chrono::milliseconds(100));
    ok &= check(beacon.publish_if_due(stub_clock.now(), machine.mode(), machine.fault_latched(), 250U, 0U, 0U),
                "Health beacon publish should succeed");

    bool saw_beacon = false;
    for (const auto& event : transport.events)
    {
        if (event.topic_view() == safety_core::diag::topic::kHealthBeacon)
        {
            saw_beacon = true;
            ok &= check(event.payload_view().find("fault=1") != std::string_view::npos,
                        "Health beacon payload should report latched fault");
            break;
        }
    }
    ok &= check(saw_beacon, "Expected health.beacon diagnostic event");

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] system context tests" << '\n';
    return 0;
}
