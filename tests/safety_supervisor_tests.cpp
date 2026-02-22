#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/topics.hpp"
#include "safety_core/safety/safety_supervisor.hpp"
#include "safety_core/state_machine/state_machine.hpp"

#include <iostream>
#include <string_view>
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

} // namespace

int main()
{
    using safety_core::safety::MonitorEvent;
    using safety_core::safety::MonitorSeverity;
    using safety_core::safety::MonitorType;
    using safety_core::safety::SafetySupervisor;
    using safety_core::sm::Mode;
    using safety_core::sm::ModeStateMachine;

    bool ok = true;

    CaptureTransport transport;
    ModeStateMachine machine(nullptr, &transport);
    (void)machine.transition_to(Mode::Idle);
    (void)machine.transition_to(Mode::Moving);

    SafetySupervisor supervisor(&machine, &transport, nullptr);

    const MonitorEvent warning_event{MonitorType::DiagnosticDrop, MonitorSeverity::Warning, 77U, "drop warning"};
    ok &= check(supervisor.process_monitor_event(warning_event).ok(), "warning processing should succeed");
    ok &= check(machine.mode() == Mode::Moving, "warning should not change mode");

    const MonitorEvent degraded_event{MonitorType::LocalizationStale, MonitorSeverity::Degraded, 88U, "stale"};
    ok &= check(supervisor.process_monitor_event(degraded_event).ok(), "degraded processing should succeed");
    ok &= check(machine.mode() == Mode::Degraded, "degraded event should request Degraded mode");

    const MonitorEvent critical_event{MonitorType::ClockRegression, MonitorSeverity::Critical, 99U, "clock backstep"};
    const auto critical_result = supervisor.process_monitor_event(critical_event);
    ok &= check(critical_result.code == safety_core::StatusCode::kFault,
                "critical processing should latch a fault and return fault result");
    ok &= check(machine.mode() == Mode::SafeStop, "critical event should force SafeStop");
    ok &= check(machine.fault_latched(), "critical event should latch fault flag");

    SafetySupervisor clock_supervisor(nullptr, &transport, nullptr);
    ok &= check(clock_supervisor.observe_clock_sample(1000U).ok(), "first clock sample should initialize state");
    const auto clock_regression = clock_supervisor.observe_clock_sample(900U);
    ok &= check(clock_regression.code == safety_core::StatusCode::kFault,
                "clock regression should trigger critical escalation");

    bool saw_warning_topic  = false;
    bool saw_degraded_topic = false;
    bool saw_safestop_topic = false;
    for (const auto& event : transport.events)
    {
        saw_warning_topic =
            saw_warning_topic || (event.topic_view() == safety_core::diag::topic::kSafetyMonitorWarning);
        saw_degraded_topic =
            saw_degraded_topic || (event.topic_view() == safety_core::diag::topic::kSafetyDegradedRequest);
        saw_safestop_topic =
            saw_safestop_topic || (event.topic_view() == safety_core::diag::topic::kSafetySafeStopForced);
    }

    ok &= check(saw_warning_topic, "warning topic should be published");
    ok &= check(saw_degraded_topic, "degraded request topic should be published");
    ok &= check(saw_safestop_topic, "safestop forced topic should be published");

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] safety supervisor tests" << '\n';
    return 0;
}
