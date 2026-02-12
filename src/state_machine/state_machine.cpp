#include "safety_core/state_machine/state_machine.hpp"

namespace safety_core::sm
{

    ModeStateMachine::ModeStateMachine() noexcept = default;

    Result ModeStateMachine::transition_to(Mode target) noexcept
    {
        if (latched_fault_ && target != Mode::SafeStop)
        {
            return Result::InvalidState("fault latched; only SafeStop allowed");
        }

        if (!allowed(target))
        {
            return Result::InvalidState("transition not allowed");
        }

        mode_ = target;
        return Result::Ok();
    }

    Result ModeStateMachine::latch_fault(std::uint16_t fault_code) noexcept
    {
        latched_fault_ = true;
        fault_code_    = fault_code;
        mode_          = Mode::SafeStop;
        return Result::Fault("fault latched");
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
        if (mode_ != Mode::LocalizationLost)
        {
            return Result::InvalidState("not in LocalizationLost");
        }
        mode_ = Mode::Degraded;
        return Result::Ok();
    }

    bool ModeStateMachine::allowed(Mode target) const noexcept
    {
        if (mode_ == target)
        {
            return true;
        }

        switch (mode_)
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

} // namespace safety_core::sm
