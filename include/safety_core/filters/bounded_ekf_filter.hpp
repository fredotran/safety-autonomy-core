#pragma once

#include "safety_core/config/system_config.hpp"

namespace safety_core::filters
{

    // Commentary: Bounded EKF is a 1D position/velocity estimator with explicit numeric clamps.
    struct BoundedEkfParams
    {
        double process_noise_position{1e-3};
        double process_noise_velocity{1e-2};
        double measurement_noise{1e-2};
        double max_abs_position{1000.0};
        double max_abs_velocity{100.0};
        double min_variance{1e-9};
        double max_variance{1e6};
    };

    class BoundedEkfFilter
    {
      public:
        void apply_config(const config::SystemConfig& cfg) noexcept
        {
            config_ = &cfg;
        }

        void set_params(const BoundedEkfParams& params) noexcept
        {
            params_ = params;
            clamp_covariance();
            x_ = clamp_position(x_);
            v_ = clamp_velocity(v_);
        }

        void reset(double position = 0.0, double velocity = 0.0) noexcept;

        bool update(double position_measurement, double assumed_accel_mps2 = 0.0) noexcept;

        [[nodiscard]] double position() const noexcept
        {
            return x_;
        }
        [[nodiscard]] double velocity() const noexcept
        {
            return v_;
        }
        [[nodiscard]] bool healthy() const noexcept
        {
            return healthy_;
        }

      private:
        [[nodiscard]] double dt_seconds() const noexcept;
        void predict(double assumed_accel_mps2) noexcept;
        void clamp_covariance() noexcept;
        [[nodiscard]] double clamp_position(double value) const noexcept;
        [[nodiscard]] double clamp_velocity(double value) const noexcept;

        const config::SystemConfig* config_{nullptr};
        BoundedEkfParams params_{};

        double x_{0.0};
        double v_{0.0};

        double p00_{1.0};
        double p01_{0.0};
        double p10_{0.0};
        double p11_{1.0};

        bool healthy_{true};
    };

} // namespace safety_core::filters
