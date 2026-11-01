// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/safety/safety_supervisor.hpp"

#include "safety_core/common/time.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/topics.hpp"
#include "safety_core/platform/clock.hpp"
#include "safety_core/state_machine/state_machine.hpp"

#include <chrono>
#include <cstdio>

namespace safety_core::safety
{

    namespace
    {
        static_assert(128U < diag::kDiagnosticPayloadCapacity);

        constexpr std::uint16_t kFaultCodeClockRegression  = 1001U;
        constexpr std::uint16_t kFaultCodeLocalizationLost = 1002U;
        constexpr std::uint16_t kFaultCodeTransportDrop    = 1003U;

        constexpr MonitorEvent make_monitor_event(MonitorType type, MonitorSeverity severity, std::uint16_t code,
                                                  std::string_view detail) noexcept
        {
            return MonitorEvent{type, severity, code, detail};
        }

        constexpr const char* monitor_name(MonitorType type) noexcept
        {
            switch (type)
            {
            case MonitorType::ClockRegression:
                return "clock_regression";
            case MonitorType::LocalizationStale:
                return "localization_stale";
            case MonitorType::TrajectoryEnvelope:
                return "trajectory_envelope";
            case MonitorType::DiagnosticDrop:
                return "diagnostic_drop";
            default:
                return "unknown";
            }
        }
    } // namespace

    SafetySupervisor::SafetySupervisor(sm::ModeStateMachine* machine, diag::DiagnosticTransport* transport,
                                       platform::Clock* clock) noexcept
        : machine_(machine), transport_(transport), clock_(clock)
    {
    }

    void SafetySupervisor::set_state_machine(sm::ModeStateMachine* machine) noexcept
    {
        machine_ = machine;
    }

    void SafetySupervisor::set_transport(diag::DiagnosticTransport* transport) noexcept
    {
        transport_ = transport;
    }

    void SafetySupervisor::set_clock(platform::Clock* clock) noexcept
    {
        clock_ = clock;
    }

    Result SafetySupervisor::process_monitor_event(const MonitorEvent& event) noexcept
    {
        switch (event.severity)
        {
        case MonitorSeverity::None:
            return Result::Ok();
        case MonitorSeverity::Warning:
            return escalate_warning(event);
        case MonitorSeverity::Degraded:
            return escalate_degraded(event);
        case MonitorSeverity::Critical:
            return escalate_critical(event);
        default:
            return Result::Fault("unknown monitor severity");
        }
    }

    Result SafetySupervisor::observe_clock_sample(std::uint64_t now_ns) noexcept
    {
        if (!has_clock_sample_)
        {
            has_clock_sample_ = true;
            last_clock_ns_    = now_ns;
            return Result::Ok();
        }

        if (now_ns < last_clock_ns_)
        {
            const MonitorEvent event = make_monitor_event(MonitorType::ClockRegression, MonitorSeverity::Critical,
                                                          kFaultCodeClockRegression, "clock moved backwards");
            last_clock_ns_           = now_ns;
            return process_monitor_event(event);
        }

        last_clock_ns_ = now_ns;
        return Result::Ok();
    }

    Result SafetySupervisor::observe_localization_age(std::uint64_t localization_age_ns,
                                                      std::uint64_t localization_timeout_ns) noexcept
    {
        if ((localization_timeout_ns == 0U) || (localization_age_ns <= localization_timeout_ns))
        {
            return Result::Ok();
        }

        const MonitorSeverity severity = (localization_age_ns >= (2U * localization_timeout_ns))
                                             ? MonitorSeverity::Critical
                                             : MonitorSeverity::Degraded;

        const MonitorEvent event = make_monitor_event(MonitorType::LocalizationStale, severity,
                                                      kFaultCodeLocalizationLost, "localization age exceeded timeout");
        return process_monitor_event(event);
    }

    Result SafetySupervisor::observe_transport_drop_count(std::uint64_t dropped_since_last_sample,
                                                          std::uint64_t warning_threshold,
                                                          std::uint64_t critical_threshold) noexcept
    {
        if ((critical_threshold > 0U) && (dropped_since_last_sample >= critical_threshold))
        {
            const MonitorEvent event =
                make_monitor_event(MonitorType::DiagnosticDrop, MonitorSeverity::Critical, kFaultCodeTransportDrop,
                                   "diagnostic drop critical threshold exceeded");
            return process_monitor_event(event);
        }

        if ((warning_threshold > 0U) && (dropped_since_last_sample >= warning_threshold))
        {
            const MonitorEvent event =
                make_monitor_event(MonitorType::DiagnosticDrop, MonitorSeverity::Warning, kFaultCodeTransportDrop,
                                   "diagnostic drop warning threshold exceeded");
            return process_monitor_event(event);
        }

        return Result::Ok();
    }

    Result SafetySupervisor::escalate_warning(const MonitorEvent& event) noexcept
    {
        publish_monitor_event(diag::topic::kSafetyMonitorWarning, event);
        return Result::Ok();
    }

    Result SafetySupervisor::escalate_degraded(const MonitorEvent& event) noexcept
    {
        publish_monitor_event(diag::topic::kSafetyDegradedRequest, event);

        // Cache pointer to prevent race condition where machine_ could become null
        auto* machine = machine_;
        if (machine == nullptr)
        {
            return Result::Ok();
        }

        const auto res = machine->transition_to(sm::Mode::Degraded);
        if (!res.ok() && (machine->mode() != sm::Mode::Degraded) && (machine->mode() != sm::Mode::SafeStop))
        {
            return Result::Fault("failed to request degraded mode");
        }
        return Result::Ok();
    }

    Result SafetySupervisor::escalate_critical(const MonitorEvent& event) noexcept
    {
        publish_monitor_event(diag::topic::kSafetySafeStopForced, event);

        // Cache pointer to prevent race condition where machine_ could become null
        auto* machine = machine_;
        if (machine == nullptr)
        {
            return Result::Fault("critical event without state machine");
        }

        const auto res = machine->latch_fault(event.code);
        if (res.code != StatusCode::kFault)
        {
            return Result::Fault("critical escalation did not latch fault");
        }
        return res;
    }

    void SafetySupervisor::publish_monitor_event(std::string_view topic, const MonitorEvent& event) const noexcept
    {
        // Cache pointers to prevent race conditions
        auto* transport   = transport_;
        const auto* clock = clock_;
        if (transport == nullptr)
        {
            return;
        }

        char payload[128]{};
        static_cast<void>(std::snprintf(payload, sizeof(payload), "monitor=%s severity=%u code=%u detail=%.*s",
                                        monitor_name(event.type), static_cast<unsigned>(event.severity),
                                        static_cast<unsigned>(event.code), static_cast<int>(event.detail.size()),
                                        event.detail.data()));

        const time::TimePoint stamp = (clock != nullptr) ? clock->now() : time::now();

        diag::DiagnosticEvent diag_event{};
        diag_event.set_topic(topic);
        diag_event.set_payload(payload);
        diag_event.timestamp_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(stamp.time_since_epoch()).count());
        transport->publish(diag_event);
    }

} // namespace safety_core::safety
