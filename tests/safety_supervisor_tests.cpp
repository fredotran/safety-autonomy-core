#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/topics.hpp"
#include "safety_core/platform/manual_clock.hpp"
#include "safety_core/safety/safety_supervisor.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "test_support.hpp"

#include <iostream>
#include <string_view>

namespace
{
    using safety_core::test_support::check;
    using CaptureTransport = safety_core::test_support::CaptureTransport;

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
    safety_core::platform::ManualClock clock;
    supervisor.set_clock(&clock);
    supervisor.set_state_machine(&machine);
    supervisor.set_transport(&transport);

    const MonitorEvent none_event{MonitorType::ClockRegression, MonitorSeverity::None, 12U, "no-op"};
    ok &= check(supervisor.process_monitor_event(none_event).ok(), "none-severity events should be ignored");

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

    ModeStateMachine monitor_machine(nullptr, &transport);
    (void)monitor_machine.transition_to(Mode::Idle);
    (void)monitor_machine.transition_to(Mode::Moving);
    SafetySupervisor monitor_supervisor(&monitor_machine, &transport, nullptr);
    ok &= check(monitor_supervisor.observe_localization_age(0U, 0U).ok(),
                "zero timeout should be treated as disabled localization monitor");
    ok &= check(monitor_supervisor.observe_localization_age(10U, 50U).ok(),
                "localization age below timeout should not escalate");
    ok &= check(monitor_supervisor.observe_localization_age(60U, 50U).ok(),
                "localization age above timeout should request degraded mode");
    ok &= check(monitor_machine.mode() == Mode::Degraded, "degraded localization should request Degraded mode");

    ModeStateMachine critical_machine(nullptr, &transport);
    (void)critical_machine.transition_to(Mode::Idle);
    (void)critical_machine.transition_to(Mode::Moving);
    SafetySupervisor critical_supervisor(&critical_machine, &transport, nullptr);
    const auto localization_critical = critical_supervisor.observe_localization_age(120U, 50U);
    ok &= check(localization_critical.code == safety_core::StatusCode::kFault,
                "localization age >= 2x timeout should force critical escalation");
    ok &= check(critical_machine.mode() == Mode::SafeStop, "critical localization staleness should force SafeStop");

    SafetySupervisor drop_supervisor(&machine, &transport, nullptr);
    ok &= check(drop_supervisor.observe_transport_drop_count(1U, 3U, 5U).ok(),
                "drop count below warning threshold should not escalate");
    ok &= check(drop_supervisor.observe_transport_drop_count(3U, 3U, 5U).ok(),
                "drop count at warning threshold should emit warning");

    SafetySupervisor critical_without_machine(nullptr, &transport, nullptr);
    const auto drop_critical = critical_without_machine.observe_transport_drop_count(5U, 3U, 5U);
    ok &= check(drop_critical.code == safety_core::StatusCode::kFault,
                "critical transport drop without state machine should return fault");

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
