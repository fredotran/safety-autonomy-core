#pragma once

#include "safety_core/config/system_config.hpp"

#include <cstddef>
#include <cstdint>

namespace safety_core::motion
{

    // Commentary: Primitive points and validator are fixed-buffer friendly to keep deterministic runtime behavior.
    struct TrajectoryPoint
    {
        double t_s{0.0};
        double speed_mps{0.0};
        double accel_mps2{0.0};
        double jerk_mps3{0.0};
        double distance_m{0.0};
    };

    enum class TrajectoryViolation : std::uint8_t
    {
        None = 0U,
        NullInput,
        EmptyTrajectory,
        NonMonotonicTime,
        SpeedOutOfBounds,
        AccelOutOfBounds,
        JerkOutOfBounds,
        NonMonotonicDistance,
    };

    struct TrajectoryValidationResult
    {
        bool ok{true};
        TrajectoryViolation violation{TrajectoryViolation::None};
        std::size_t failing_index{0U};
    };

    class TrajectoryValidator
    {
      public:
        void apply_config(const config::SystemConfig& cfg) noexcept
        {
            config_ = &cfg;
        }

        void set_max_jerk_mps3(double jerk_limit) noexcept
        {
            max_jerk_mps3_ = (jerk_limit < 0.0) ? 0.0 : jerk_limit;
        }

        [[nodiscard]] TrajectoryValidationResult validate(const TrajectoryPoint* points,
                                                          std::size_t count) const noexcept;

      private:
        const config::SystemConfig* config_{nullptr};
        double max_jerk_mps3_{5.0};
    };

    bool generate_emergency_stop_profile(double initial_speed_mps, double max_decel_mps2, double dt_s,
                                         TrajectoryPoint* out_points, std::size_t capacity,
                                         std::size_t& out_count) noexcept;

} // namespace safety_core::motion
