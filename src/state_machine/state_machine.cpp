#include "safety_core/state_machine/state_machine.hpp"

#include "safety_core/diag/health_monitor.hpp"
#include "safety_core/system/system_context.hpp"

namespace safety_core::sm
{

    ModeStateMachine::ModeStateMachine(diag::HealthMonitor* monitor) noexcept : monitor_(monitor) {}

    ModeStateMachine::ModeStateMachine(const system::SystemContext& context) noexcept : monitor_(context.health_monitor)
    {
    }

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

        const Mode prev = mode_;
        mode_           = target;
        notify_transition(prev, target);
        return Result::Ok();
    }

    Result ModeStateMachine::latch_fault(std::uint16_t fault_code) noexcept
    {
        latched_fault_ = true;
        fault_code_    = fault_code;
        mode_          = Mode::SafeStop;
        notify_fault(fault_code);
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

    void ModeStateMachine::set_monitor(diag::HealthMonitor* monitor) noexcept
    {
        monitor_ = monitor;
    }

    void ModeStateMachine::notify_transition(Mode from, Mode to) const noexcept
    {
        if ((monitor_ == nullptr) || (from == to))
        {
            return;
        }
        monitor_->on_mode_transition(from, to);
    }

    void ModeStateMachine::notify_fault(std::uint16_t fault_code) const noexcept
    {
        if (monitor_ == nullptr)
        {
            return;
        }
        monitor_->on_fault_latched(fault_code);
    }

} // namespace safety_core::sm
