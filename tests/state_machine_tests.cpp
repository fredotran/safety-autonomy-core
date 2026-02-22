#include "safety_core/state_machine/state_machine.hpp"
#include "test_support.hpp"

#include <iostream>

namespace
{
    using safety_core::test_support::check;

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

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] state machine smoke tests" << '\n';
    return 0;
}
