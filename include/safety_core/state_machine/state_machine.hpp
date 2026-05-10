#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/result.hpp"

#include <atomic>
#include <cstdint>
#include <string_view>

namespace safety_core::diag
{
    class HealthMonitor;
    class DiagnosticTransport;
} // namespace safety_core::diag

namespace safety_core::system
{
    class SystemContext;
}

namespace safety_core::platform
{
    class Clock;
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
        explicit ModeStateMachine(diag::HealthMonitor* monitor         = nullptr,
                                  diag::DiagnosticTransport* transport = nullptr) noexcept;
        explicit ModeStateMachine(const system::SystemContext& context) noexcept;

        [[nodiscard]] Mode mode() const noexcept
        {
            return mode_.load(std::memory_order_acquire);
        }
        [[nodiscard]] bool fault_latched() const noexcept
        {
            return latched_fault_.load(std::memory_order_acquire);
        }

        Result transition_to(Mode target) noexcept;
        Result latch_fault(std::uint16_t fault_code) noexcept;
        Result request_obstacle_hold() noexcept;
        Result report_localization_lost() noexcept;
        Result recover_localization() noexcept;
        void set_monitor(diag::HealthMonitor* monitor) noexcept;
        void set_diagnostic_transport(diag::DiagnosticTransport* transport) noexcept;
        void set_clock(platform::Clock* clock) noexcept;

      private:
        [[nodiscard]] bool allowed(Mode target) const noexcept;
        void notify_transition(Mode from, Mode to) const noexcept;
        void notify_fault(std::uint16_t fault_code) const noexcept;
        void publish_event(std::string_view topic, std::string_view payload) const noexcept;

        std::atomic<Mode> mode_{Mode::Init};
        std::atomic<bool> latched_fault_{false};
        std::uint16_t fault_code_{0U};
        diag::HealthMonitor* monitor_{nullptr};
        diag::DiagnosticTransport* diag_transport_{nullptr};
        platform::Clock* clock_{nullptr};
    };

} // namespace safety_core::sm
