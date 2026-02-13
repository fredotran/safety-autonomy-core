#pragma once

#include "safety_core/result.hpp"

#include <cstdint>

namespace safety_core::diag
{
    class HealthMonitor;
}

namespace safety_core::system
{
    class SystemContext;
}

namespace safety_core::sm
{

    enum class Mode : std::uint8_t
    {
        Init = 0U,
        Idle,
        Moving,
        Degraded,
        AvoidingObstacle, // Temporary stop due to dynamic obstacle
        LocalizationLost, // Safe-limited behavior while localization is uncertain
        Docking,
        SafeStop,
    };

    struct Transition
    {
        Mode from;
        Mode to;
    };

    class ModeStateMachine
    {
      public:
        explicit ModeStateMachine(diag::HealthMonitor* monitor = nullptr) noexcept;
        explicit ModeStateMachine(const system::SystemContext& context) noexcept;

        [[nodiscard]] Mode mode() const noexcept
        {
            return mode_;
        }
        [[nodiscard]] bool fault_latched() const noexcept
        {
            return latched_fault_;
        }

        Result transition_to(Mode target) noexcept;
        Result latch_fault(std::uint16_t fault_code) noexcept;
        Result request_obstacle_hold() noexcept;
        Result report_localization_lost() noexcept;
        Result recover_localization() noexcept;
        void set_monitor(diag::HealthMonitor* monitor) noexcept;

      private:
        [[nodiscard]] bool allowed(Mode target) const noexcept;
        void notify_transition(Mode from, Mode to) const noexcept;
        void notify_fault(std::uint16_t fault_code) const noexcept;

        Mode mode_{Mode::Init};
        bool latched_fault_{false};
        std::uint16_t fault_code_{0U};
        diag::HealthMonitor* monitor_{nullptr};
    };

} // namespace safety_core::sm
