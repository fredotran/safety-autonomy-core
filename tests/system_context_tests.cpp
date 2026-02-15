#include "safety_core/diag/logging_health_monitor.hpp"
#include "safety_core/exec/task_executor.hpp"
#include "safety_core/platform/clock.hpp"
#include "safety_core/safety/safety_envelope.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "safety_core/system/system_context.hpp"

#include <chrono>
#include <iostream>
#include <sstream>

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
    using safety_core::diag::LoggingHealthMonitor;
    using safety_core::exec::TaskExecutor;
    using safety_core::platform::Clock;
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

    // Logging monitor integration via SystemContext constructor.
    StubClock monitor_clock;
    std::ostringstream log_sink;
    LoggingHealthMonitor monitor(log_sink, &monitor_clock);
    SystemContext context{&config, &monitor_clock, &monitor};

    ModeStateMachine machine(context);
    ok &= check(machine.transition_to(Mode::Idle).ok(), "Init -> Idle via context");
    machine.latch_fault(7U);
    const std::string logs = log_sink.str();
    ok &= check(logs.find("ts=") != std::string::npos, "Monitor should emit timestamps");
    ok &= check(logs.find("mode_transition") != std::string::npos, "Monitor should log transitions");
    ok &= check(logs.find("fault_latched") != std::string::npos, "Monitor should log faults");

    // TaskExecutor wiring with config + stub clock.
    StubClock stub_clock;
    TaskExecutor<2U> executor(&stub_clock);
    ok &= check(executor.apply_config(config).ok(), "Executor accepts config");
    bool ran = false;
    FlagTaskContext task_ctx{&ran};
    ok &= check(executor.add_task(mark_ran_task, &task_ctx, safety_core::time::Duration::zero()).ok(),
                "Task add should succeed");

    stub_clock.advance(config.timing.control_period);
    ok &= check(executor.run_due(stub_clock.now()).ok(), "Executor run should succeed");
    ok &= check(ran, "Task should have executed");

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] system context tests" << '\n';
    return 0;
}
