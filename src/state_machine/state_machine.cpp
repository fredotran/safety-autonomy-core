#include "safety_core/state_machine/state_machine.hpp"

namespace safety_core::sm {

ModeStateMachine::ModeStateMachine() noexcept = default;

Result ModeStateMachine::transition_to(Mode target) noexcept {
    if (latched_fault_ && target != Mode::kSafeStop) {
        return Result::InvalidState("fault latched; only SafeStop allowed");
    }

    if (!allowed(target)) {
        return Result::InvalidState("transition not allowed");
    }

    mode_ = target;
    return Result::Ok();
}

Result ModeStateMachine::latch_fault(std::uint16_t fault_code) noexcept {
    latched_fault_ = true;
    fault_code_ = fault_code;
    mode_ = Mode::kSafeStop;
    return Result::Fault("fault latched");
}

Result ModeStateMachine::request_obstacle_hold() noexcept {
    return transition_to(Mode::kObstacleHold);
}

Result ModeStateMachine::report_localization_lost() noexcept {
    return transition_to(Mode::kLocalizationLost);
}

Result ModeStateMachine::recover_localization() noexcept {
    // Recovery goes to Degraded to avoid immediate full-performance until validated.
    if (mode_ != Mode::kLocalizationLost) {
        return Result::InvalidState("not in LocalizationLost");
    }
    mode_ = Mode::kDegraded;
    return Result::Ok();
}

bool ModeStateMachine::allowed(Mode target) const noexcept {
    if (mode_ == target) {
        return true;
    }

    switch (mode_) {
        case Mode::kInit:
            return target == Mode::kStandby || target == Mode::kSafeStop;
        case Mode::kStandby:
            return target == Mode::kActive || target == Mode::kDegraded || target == Mode::kSafeStop;
        case Mode::kActive:
            return target == Mode::kDegraded || target == Mode::kObstacleHold || target == Mode::kLocalizationLost ||
                   target == Mode::kSafeStop;
        case Mode::kDegraded:
            return target == Mode::kActive || target == Mode::kObstacleHold || target == Mode::kLocalizationLost ||
                   target == Mode::kSafeStop;
        case Mode::kObstacleHold:
            return target == Mode::kActive || target == Mode::kDegraded || target == Mode::kSafeStop;
        case Mode::kLocalizationLost:
            return target == Mode::kDegraded || target == Mode::kStandby || target == Mode::kSafeStop;
        case Mode::kSafeStop:
            return target == Mode::kSafeStop;  // stay in safe stop once entered.
        default:
            return false;
    }
}

}  // namespace safety_core::sm
