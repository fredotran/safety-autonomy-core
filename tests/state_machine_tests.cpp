#include "safety_core/platform/clock.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "safety_core/system/system_context.hpp"
#include "test_support.hpp"

#include <iostream>
#include <string_view>

namespace
{
    using safety_core::test_support::CaptureTransport;
    using safety_core::test_support::check;

    class RecordingMonitor final : public safety_core::diag::HealthMonitor
    {
      public:
        void on_mode_transition(safety_core::sm::Mode from, safety_core::sm::Mode to) noexcept override
        {
            ++transition_calls;
            last_from = from;
            last_to   = to;
        }

        void on_fault_latched(std::uint16_t fault_code) noexcept override
        {
            ++fault_calls;
            last_fault_code = fault_code;
        }

        int transition_calls{0};
        int fault_calls{0};
        safety_core::sm::Mode last_from{safety_core::sm::Mode::Init};
        safety_core::sm::Mode last_to{safety_core::sm::Mode::Init};
        std::uint16_t last_fault_code{0U};
    };

    class FixedClock final : public safety_core::platform::Clock
    {
      public:
        [[nodiscard]] safety_core::time::TimePoint now() const noexcept override
        {
            return safety_core::time::TimePoint{stamp};
        }

        std::chrono::nanoseconds stamp{std::chrono::nanoseconds{123456789}};
    };

    bool has_event(const CaptureTransport& transport, std::string_view topic)
    {
        for (const auto& event : transport.events)
        {
            if (event.topic_view() == topic)
            {
                return true;
            }
        }
        return false;
    }
} // namespace

int main()
{
    using safety_core::Result;
    using safety_core::sm::Mode;
    using safety_core::sm::ModeStateMachine;

    ModeStateMachine sm;

    bool ok = true;
    ok &= check(sm.mode() == Mode::Init, "initial mode should be Init");

    Result res = sm.transition_to(Mode::Idle);
    ok &= check(res.ok(), "Init -> Idle should succeed");
    ok &= check(sm.mode() == Mode::Idle, "mode should be Idle");

    res = sm.transition_to(Mode::Moving);
    ok &= check(res.ok(), "Idle -> Moving should succeed");
    ok &= check(sm.mode() == Mode::Moving, "mode should be Moving");

    res = sm.transition_to(Mode::Init);
    ok &= check(!res.ok(), "Moving -> Init should be rejected");
    ok &= check(sm.mode() == Mode::Moving, "mode should remain Moving after rejected transition");

    res = sm.transition_to(Mode::Degraded);
    ok &= check(res.ok(), "Moving -> Degraded should succeed");
    ok &= check(sm.mode() == Mode::Degraded, "mode should be Degraded");

    res = sm.request_obstacle_hold();
    ok &= check(res.ok(), "Degraded -> AvoidingObstacle should succeed");
    ok &= check(sm.mode() == Mode::AvoidingObstacle, "mode should be AvoidingObstacle");

    res = sm.transition_to(Mode::Degraded);
    ok &= check(res.ok(), "AvoidingObstacle -> Degraded should succeed");
    ok &= check(sm.mode() == Mode::Degraded, "mode should be Degraded after obstacle cleared");

    res = sm.report_localization_lost();
    ok &= check(res.ok(), "Degraded -> LocalizationLost should succeed");
    ok &= check(sm.mode() == Mode::LocalizationLost, "mode should be LocalizationLost");

    res = sm.recover_localization();
    ok &= check(res.ok(), "recover_localization should exit LocalizationLost to Degraded");
    ok &= check(sm.mode() == Mode::Degraded, "mode should be Degraded after localization recovery");

    res = sm.transition_to(Mode::Moving);
    ok &= check(res.ok(), "Degraded -> Moving should succeed after recovery");
    ok &= check(sm.mode() == Mode::Moving, "mode should be Moving again");

    res = sm.latch_fault(42U);
    ok &= check(!res.ok(), "latch_fault should return error result");
    ok &= check(sm.mode() == Mode::SafeStop, "fault should force SafeStop mode");
    ok &= check(sm.fault_latched(), "fault latch should be set");

    res = sm.transition_to(Mode::Moving);
    ok &= check(!res.ok(), "cannot leave SafeStop after latched fault");
    ok &= check(sm.mode() == Mode::SafeStop, "mode should remain SafeStop after failed transition");

    // ------------------------------------------------------------------
    // Fault lifecycle
    // ------------------------------------------------------------------
    {
        ModeStateMachine fsm;
        ok &= check(fsm.clear_fault().ok() == false, "clear_fault without latched fault should fail");

        static_cast<void>(fsm.latch_fault(7U));
        ok &= check(fsm.mode() == Mode::SafeStop, "latched fault forces SafeStop");
        ok &= check(fsm.clear_fault().ok(), "clear_fault with latched fault should succeed");
        ok &= check(!fsm.fault_latched(), "fault latch should be cleared");
        ok &= check(fsm.mode() == Mode::Idle, "clear_fault should transition to Idle");
    }

    // SafeStop re-entry while a fault is latched must remain legal.
    {
        ModeStateMachine fsm;
        static_cast<void>(fsm.latch_fault(9U));
        res = fsm.transition_to(Mode::SafeStop);
        ok &= check(res.ok(), "SafeStop -> SafeStop with latched fault should be allowed");
    }

    // recover_localization from any other mode must fail.
    {
        ModeStateMachine fsm;
        res = fsm.recover_localization();
        ok &= check(!res.ok(), "recover_localization outside LocalizationLost should fail");

        static_cast<void>(fsm.transition_to(Mode::Idle));
        res = fsm.recover_localization();
        ok &= check(!res.ok(), "recover_localization from Idle should fail");
        ok &= check(fsm.mode() == Mode::Idle, "failed recovery should leave mode unchanged");
    }

    // Idempotent transitions are no-ops and must succeed from every mode.
    {
        ModeStateMachine fsm;
        res = fsm.transition_to(Mode::Init);
        ok &= check(res.ok(), "Init -> Init should succeed (idempotent)");
        ok &= check(fsm.mode() == Mode::Init, "idempotent transition should not change mode");

        static_cast<void>(fsm.transition_to(Mode::Idle));
        res = fsm.transition_to(Mode::Idle);
        ok &= check(res.ok(), "Idle -> Idle should succeed (idempotent)");
    }

    // ------------------------------------------------------------------
    // Transition matrix: every allowed edge plus representative rejections
    // ------------------------------------------------------------------
    {
        struct Edge
        {
            Mode from;
            Mode target;
            bool allowed;
        };

        // Helper to drive fsm into a source state via a legal path.
        const auto drive_to = [](ModeStateMachine& m, Mode target)
        {
            switch (target)
            {
            case Mode::Init:
                break; // fresh instance
            case Mode::Idle:
                static_cast<void>(m.transition_to(Mode::Idle));
                break;
            case Mode::Moving:
                static_cast<void>(m.transition_to(Mode::Idle));
                static_cast<void>(m.transition_to(Mode::Moving));
                break;
            case Mode::Degraded:
                static_cast<void>(m.transition_to(Mode::Idle));
                static_cast<void>(m.transition_to(Mode::Degraded));
                break;
            case Mode::AvoidingObstacle:
                static_cast<void>(m.transition_to(Mode::Idle));
                static_cast<void>(m.transition_to(Mode::Moving));
                static_cast<void>(m.transition_to(Mode::AvoidingObstacle));
                break;
            case Mode::LocalizationLost:
                static_cast<void>(m.transition_to(Mode::Idle));
                static_cast<void>(m.transition_to(Mode::Moving));
                static_cast<void>(m.transition_to(Mode::LocalizationLost));
                break;
            case Mode::Docking:
                static_cast<void>(m.transition_to(Mode::Idle));
                static_cast<void>(m.transition_to(Mode::Docking));
                break;
            case Mode::SafeStop:
                static_cast<void>(m.transition_to(Mode::SafeStop));
                break;
            }
        };

        const Edge edges[] = {
            {Mode::Init, Mode::Idle, true},
            {Mode::Init, Mode::SafeStop, true},
            {Mode::Init, Mode::Moving, false},
            {Mode::Init, Mode::Docking, false},
            {Mode::Idle, Mode::Moving, true},
            {Mode::Idle, Mode::Degraded, true},
            {Mode::Idle, Mode::SafeStop, true},
            {Mode::Idle, Mode::Docking, true},
            {Mode::Idle, Mode::AvoidingObstacle, false},
            {Mode::Idle, Mode::LocalizationLost, false},
            {Mode::Moving, Mode::Degraded, true},
            {Mode::Moving, Mode::AvoidingObstacle, true},
            {Mode::Moving, Mode::LocalizationLost, true},
            {Mode::Moving, Mode::SafeStop, true},
            {Mode::Moving, Mode::Docking, true},
            {Mode::Moving, Mode::Idle, false},
            {Mode::Moving, Mode::Init, false},
            {Mode::Degraded, Mode::Moving, true},
            {Mode::Degraded, Mode::AvoidingObstacle, true},
            {Mode::Degraded, Mode::LocalizationLost, true},
            {Mode::Degraded, Mode::SafeStop, true},
            {Mode::Degraded, Mode::Docking, true},
            {Mode::Degraded, Mode::Idle, false},
            {Mode::AvoidingObstacle, Mode::Moving, true},
            {Mode::AvoidingObstacle, Mode::Degraded, true},
            {Mode::AvoidingObstacle, Mode::SafeStop, true},
            {Mode::AvoidingObstacle, Mode::Docking, false},
            {Mode::AvoidingObstacle, Mode::LocalizationLost, false},
            {Mode::LocalizationLost, Mode::Degraded, true},
            {Mode::LocalizationLost, Mode::Idle, true},
            {Mode::LocalizationLost, Mode::SafeStop, true},
            {Mode::LocalizationLost, Mode::Moving, false},
            {Mode::LocalizationLost, Mode::AvoidingObstacle, false},
            {Mode::Docking, Mode::Idle, true},
            {Mode::Docking, Mode::SafeStop, true},
            {Mode::Docking, Mode::Moving, false},
            {Mode::Docking, Mode::Degraded, false},
            {Mode::SafeStop, Mode::Idle, false},
            {Mode::SafeStop, Mode::Moving, false},
            {Mode::SafeStop, Mode::Degraded, false},
            {Mode::SafeStop, Mode::Docking, false},
            {Mode::SafeStop, Mode::LocalizationLost, false},
            {Mode::SafeStop, Mode::SafeStop, true},
        };

        for (const Edge& edge : edges)
        {
            ModeStateMachine m;
            drive_to(m, edge.from);
            const Mode before = m.mode();
            const Result r    = m.transition_to(edge.target);
            if (edge.allowed)
            {
                ok &= check(r.ok(), "allowed transition rejected");
                ok &= check(m.mode() == edge.target, "mode did not reach target");
            }
            else
            {
                ok &= check(!r.ok(), "illegal transition accepted");
                ok &= check(m.mode() == before, "illegal transition changed mode");
            }
        }
    }

    // ------------------------------------------------------------------
    // Notifications: monitor callbacks + diagnostic transport events
    // ------------------------------------------------------------------
    {
        RecordingMonitor monitor;
        CaptureTransport transport;
        ModeStateMachine fsm(&monitor, &transport);

        static_cast<void>(fsm.transition_to(Mode::Idle));
        ok &= check(monitor.transition_calls == 1, "monitor should observe Init->Idle");
        ok &= check(monitor.last_from == Mode::Init && monitor.last_to == Mode::Idle,
                    "monitor should record from/to modes");
        ok &= check(has_event(transport, "mode_transition"), "transition event should be published");

        // Idempotent transition publishes nothing (from == to early return).
        const std::size_t before = transport.events.size();
        static_cast<void>(fsm.transition_to(Mode::Idle));
        ok &= check(transport.events.size() == before, "idempotent transition should not publish");
        ok &= check(monitor.transition_calls == 1, "idempotent transition should not notify monitor");

        static_cast<void>(fsm.latch_fault(3U));
        ok &= check(monitor.fault_calls == 1 && monitor.last_fault_code == 3U,
                    "monitor should observe latched fault code");
        ok &= check(has_event(transport, "fault_latched"), "fault event should be published");
    }

    // Setters rewire monitor/transport/clock after construction.
    {
        RecordingMonitor monitor;
        CaptureTransport transport;
        FixedClock clock;
        ModeStateMachine fsm;
        fsm.set_monitor(&monitor);
        fsm.set_diagnostic_transport(&transport);
        fsm.set_clock(&clock);

        static_cast<void>(fsm.transition_to(Mode::Idle));
        ok &= check(monitor.transition_calls == 1, "set_monitor should install observer");
        ok &= check(!transport.events.empty(), "set_diagnostic_transport should install publisher");
        ok &= check(transport.events.back().timestamp_ns == 123456789U, "set_clock should feed event timestamps");
    }

    // SystemContext constructor wires monitor/transport/clock at once.
    {
        RecordingMonitor monitor;
        CaptureTransport transport;
        FixedClock clock;
        safety_core::system::SystemContext context{};
        context.health_monitor       = &monitor;
        context.diagnostic_transport = &transport;
        context.clock                = &clock;

        ModeStateMachine fsm(context);
        static_cast<void>(fsm.transition_to(Mode::Idle));
        ok &= check(monitor.transition_calls == 1, "context ctor should wire monitor");
        ok &= check(!transport.events.empty() && transport.events.back().timestamp_ns == 123456789U,
                    "context ctor should wire transport and clock");
    }

    // request_obstacle_hold / report_localization_lost rejections.
    {
        ModeStateMachine fsm;
        res = fsm.request_obstacle_hold();
        ok &= check(!res.ok(), "Init -> AvoidingObstacle via hold should be rejected");
        res = fsm.report_localization_lost();
        ok &= check(!res.ok(), "Init -> LocalizationLost should be rejected");
    }

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] state machine smoke tests" << '\n';
    return 0;
}
