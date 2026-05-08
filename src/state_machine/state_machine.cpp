// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/state_machine/state_machine.hpp"

#include "safety_core/common/time.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/health_monitor.hpp"
#include "safety_core/diag/topics.hpp"
#include "safety_core/system/system_context.hpp"

#include <cstdio>

namespace safety_core::sm
{

    namespace
    {
        static_assert(64U < diag::kDiagnosticPayloadCapacity);
        static_assert(48U < diag::kDiagnosticPayloadCapacity);
    } // namespace

    ModeStateMachine::ModeStateMachine(diag::HealthMonitor* monitor, diag::DiagnosticTransport* transport) noexcept
        : monitor_(monitor), diag_transport_(transport)
    {
    }

    ModeStateMachine::ModeStateMachine(const system::SystemContext& context) noexcept
        : monitor_(context.health_monitor), diag_transport_(context.diagnostic_transport), clock_(context.clock)
    {
    }

    Result ModeStateMachine::transition_to(Mode target) noexcept
    {
        if (latched_fault_.load(std::memory_order_acquire) && target != Mode::SafeStop)
        {
            return Result::InvalidState("fault latched; only SafeStop allowed");
        }

        if (!allowed(target))
        {
            return Result::InvalidState("transition not allowed");
        }

        // Atomic transition using compare-and-swap to prevent race conditions
        Mode prev = mode_.load(std::memory_order_acquire);
        while (!mode_.compare_exchange_weak(prev, target,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
        {
            // CAS failed: prev now contains the current mode
            // Re-validate that transition is still allowed from the new current state
            if (!allowed(target))
            {
                return Result::InvalidState("transition not allowed");
            }
            // If transition to same mode, allow it (idempotent)
            if (prev == target)
            {
                return Result::Ok();
            }
        }

        notify_transition(prev, target);
        return Result::Ok();
    }

    Result ModeStateMachine::latch_fault(std::uint16_t fault_code) noexcept
    {
        latched_fault_.store(true, std::memory_order_release);
        fault_code_ = fault_code;
        mode_.store(Mode::SafeStop, std::memory_order_release);
        notify_fault(fault_code);
        return Result::Fault("fault latched");
    }

    Result ModeStateMachine::clear_fault() noexcept
    {
        if (!latched_fault_.load(std::memory_order_acquire))
        {
            return Result::InvalidState("no fault latched");
        }

        latched_fault_.store(false, std::memory_order_release);
        fault_code_ = 0U;

        // Atomic transition to Idle mode after clearing fault
        Mode prev = mode_.load(std::memory_order_acquire);
        while (!mode_.compare_exchange_weak(prev, Mode::Idle,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
        {
            // CAS failed: prev now contains the current mode
            // Allow transition even if state changed (fault clearing takes precedence)
        }

        notify_transition(prev, Mode::Idle);

        return Result::Ok();
    }

    Result ModeStateMachine::request_obstacle_hold() noexcept
    {
        return transition_to(Mode::AvoidingObstacle);
    }

    Result ModeStateMachine::report_localization_lost() noexcept
    {
        return transition_to(Mode::LocalizationLost);
    }

    Result ModeStateMachine::recover_localization() noexcept
    {
        // Recovery goes to Degraded to avoid immediate full-performance until validated.
        Mode current = mode_.load(std::memory_order_acquire);
        if (current != Mode::LocalizationLost)
        {
            return Result::InvalidState("not in LocalizationLost");
        }

        // Atomic transition to Degraded mode
        Mode prev = current;
        while (!mode_.compare_exchange_weak(prev, Mode::Degraded,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
        {
            // CAS failed: state changed, check if still valid
            if (prev != Mode::LocalizationLost)
            {
                return Result::InvalidState("state changed during recovery");
            }
        }

        return Result::Ok();
    }

    bool ModeStateMachine::allowed(Mode target) const noexcept
    {
        const Mode current = mode_.load(std::memory_order_acquire);
        if (current == target)
        {
            return true;
        }

        switch (current)
        {
        case Mode::Init:
            return target == Mode::Idle || target == Mode::SafeStop;
        case Mode::Idle:
            return target == Mode::Moving || target == Mode::Degraded || target == Mode::SafeStop ||
                   target == Mode::Docking;
        case Mode::Moving:
            return target == Mode::Degraded || target == Mode::AvoidingObstacle || target == Mode::LocalizationLost ||
                   target == Mode::SafeStop || target == Mode::Docking;
        case Mode::Degraded:
            return target == Mode::Moving || target == Mode::AvoidingObstacle || target == Mode::LocalizationLost ||
                   target == Mode::SafeStop || target == Mode::Docking;
        case Mode::AvoidingObstacle:
            return target == Mode::Moving || target == Mode::Degraded || target == Mode::SafeStop;
        case Mode::LocalizationLost:
            return target == Mode::Degraded || target == Mode::Idle || target == Mode::SafeStop;
        case Mode::Docking:
            return target == Mode::Idle || target == Mode::SafeStop;
        case Mode::SafeStop:
            return target == Mode::SafeStop; // stay in safe stop once entered.
        default:
            return false;
        }
    }

    void ModeStateMachine::set_monitor(diag::HealthMonitor* monitor) noexcept
    {
        monitor_ = monitor;
    }

    void ModeStateMachine::set_diagnostic_transport(diag::DiagnosticTransport* transport) noexcept
    {
        diag_transport_ = transport;
    }

    void ModeStateMachine::set_clock(platform::Clock* clock) noexcept
    {
        clock_ = clock;
    }

    void ModeStateMachine::notify_transition(Mode from, Mode to) const noexcept
    {
        if (from == to)
        {
            return;
        }

        if (monitor_ != nullptr)
        {
            monitor_->on_mode_transition(from, to);
        }

        char payload[64]{};
        static_cast<void>(
            std::snprintf(payload, sizeof(payload), "from=%d to=%d", static_cast<int>(from), static_cast<int>(to)));
        publish_event(diag::topic::kModeTransition, std::string_view(payload));
    }

    void ModeStateMachine::notify_fault(std::uint16_t fault_code) const noexcept
    {
        if (monitor_ != nullptr)
        {
            monitor_->on_fault_latched(fault_code);
        }

        char payload[48]{};
        static_cast<void>(std::snprintf(payload, sizeof(payload), "code=%u", static_cast<unsigned>(fault_code)));
        publish_event(diag::topic::kFaultLatched, std::string_view(payload));
    }

    void ModeStateMachine::publish_event(std::string_view topic, std::string_view payload) const noexcept
    {
        if (diag_transport_ == nullptr)
        {
            return;
        }

        const time::TimePoint stamp = (clock_ != nullptr) ? clock_->now() : time::now();
        const auto timestamp        = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(stamp.time_since_epoch()).count());

        diag::DiagnosticEvent event{};
        event.set_topic(topic);
        event.set_payload(payload);
        event.timestamp_ns = timestamp;
        diag_transport_->publish(event);
    }

} // namespace safety_core::sm
