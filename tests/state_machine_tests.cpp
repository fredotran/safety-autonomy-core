#include "safety_core/state_machine/state_machine.hpp"

#include <iostream>

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

} // namespace

int main()
{
    using safety_core::Result;
    using safety_core::sm::Mode;
    using safety_core::sm::ModeStateMachine;

    ModeStateMachine sm;

    bool ok = true;
    ok &= check(sm.mode() == Mode::kInit, "initial mode should be Init");

    Result res = sm.transition_to(Mode::kStandby);
    ok &= check(res.ok(), "Init -> Standby should succeed");
    ok &= check(sm.mode() == Mode::kStandby, "mode should be Standby");

    res = sm.transition_to(Mode::kActive);
    ok &= check(res.ok(), "Standby -> Active should succeed");
    ok &= check(sm.mode() == Mode::kActive, "mode should be Active");

    res = sm.transition_to(Mode::kInit);
    ok &= check(!res.ok(), "Active -> Init should be rejected");
    ok &= check(sm.mode() == Mode::kActive, "mode should remain Active after rejected transition");

    res = sm.transition_to(Mode::kDegraded);
    ok &= check(res.ok(), "Active -> Degraded should succeed");
    ok &= check(sm.mode() == Mode::kDegraded, "mode should be Degraded");

    res = sm.request_obstacle_hold();
    ok &= check(res.ok(), "Degraded -> ObstacleHold should succeed");
    ok &= check(sm.mode() == Mode::kObstacleHold, "mode should be ObstacleHold");

    res = sm.transition_to(Mode::kDegraded);
    ok &= check(res.ok(), "ObstacleHold -> Degraded should succeed");
    ok &= check(sm.mode() == Mode::kDegraded, "mode should be Degraded after obstacle cleared");

    res = sm.report_localization_lost();
    ok &= check(res.ok(), "Degraded -> LocalizationLost should succeed");
    ok &= check(sm.mode() == Mode::kLocalizationLost, "mode should be LocalizationLost");

    res = sm.recover_localization();
    ok &= check(res.ok(), "recover_localization should exit LocalizationLost to Degraded");
    ok &= check(sm.mode() == Mode::kDegraded, "mode should be Degraded after localization recovery");

    res = sm.transition_to(Mode::kActive);
    ok &= check(res.ok(), "Degraded -> Active should succeed after recovery");
    ok &= check(sm.mode() == Mode::kActive, "mode should be Active again");

    res = sm.latch_fault(42U);
    ok &= check(!res.ok(), "latch_fault should return error result");
    ok &= check(sm.mode() == Mode::kSafeStop, "fault should force SafeStop mode");
    ok &= check(sm.fault_latched(), "fault latch should be set");

    res = sm.transition_to(Mode::kActive);
    ok &= check(!res.ok(), "cannot leave SafeStop after latched fault");
    ok &= check(sm.mode() == Mode::kSafeStop, "mode should remain SafeStop after failed transition");

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] state machine smoke tests" << '\n';
    return 0;
}
