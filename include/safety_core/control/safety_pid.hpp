#pragma once

#include "safety_core/common/time.hpp"
#include "safety_core/config/system_config.hpp"
#include "safety_core/platform/clock.hpp"
#include "safety_core/result.hpp"

namespace safety_core::control
{

    struct PidGains
    {
        double kp{0.0};
        double ki{0.0};
        double kd{0.0};
    };

    class SafetyPidController
    {
      public:
        explicit SafetyPidController(platform::Clock* clock = nullptr) noexcept;

        void set_clock(platform::Clock* clock) noexcept;
        Result apply_config(const config::SystemConfig& cfg) noexcept;
        void set_gains(const PidGains& gains) noexcept;
        void reset() noexcept;
        void mark_localization_update(time::TimePoint stamp = time::now()) noexcept;
        double compute(double setpoint, double measurement) noexcept;

      private:
        [[nodiscard]] time::TimePoint now() const noexcept;
        [[nodiscard]] bool localization_valid(time::TimePoint stamp) const noexcept;
        double clamp_speed(double value) const noexcept;
        double clamp_accel(double desired, double dt) const noexcept;

        platform::Clock* clock_{nullptr};
        const config::SystemConfig* config_{nullptr};
        PidGains gains_{};
        double integral_{0.0};
        double prev_error_{0.0};
        double last_output_{0.0};
        bool first_update_{true};
        time::TimePoint last_update_{};
        time::TimePoint last_localization_update_{};
    };

} // namespace safety_core::control
