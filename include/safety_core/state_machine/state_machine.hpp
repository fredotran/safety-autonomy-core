#pragma once

#include <array>
#include <cstdint>

#include "safety_core/result.hpp"

namespace safety_core::sm {

enum class Mode : std::uint8_t {
    kInit = 0U,
    kStandby,
    kActive,
    kDegraded,
    kObstacleHold,       // Temporary stop due to dynamic obstacle
    kLocalizationLost,   // Safe-limited behavior while localization is uncertain
    kSafeStop,
};

struct Transition {
    Mode from;
    Mode to;
};

class ModeStateMachine {
public:
    ModeStateMachine() noexcept;

    [[nodiscard]] Mode mode() const noexcept { return mode_; }
    [[nodiscard]] bool fault_latched() const noexcept { return latched_fault_; }

    Result transition_to(Mode target) noexcept;
    Result latch_fault(std::uint16_t fault_code) noexcept;
    Result request_obstacle_hold() noexcept;
    Result report_localization_lost() noexcept;
    Result recover_localization() noexcept;

private:
    [[nodiscard]] bool allowed(Mode target) const noexcept;

    Mode mode_{Mode::kInit};
    bool latched_fault_{false};
    std::uint16_t fault_code_{0U};
};

}  // namespace safety_core::sm
