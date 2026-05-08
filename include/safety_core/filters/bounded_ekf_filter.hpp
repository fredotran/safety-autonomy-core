#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/config/system_config.hpp"

#include <cmath>
#include <cstdint>

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
        double innovation_gate_sigma{5.0}; // Mahalanobis gate (0 = disabled)
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
            // Validate all floating-point parameters to prevent undefined behavior
            if (!std::isfinite(params.process_noise_position) || !std::isfinite(params.process_noise_velocity) ||
                !std::isfinite(params.measurement_noise) || !std::isfinite(params.max_abs_position) ||
                !std::isfinite(params.max_abs_velocity) || !std::isfinite(params.min_variance) ||
                !std::isfinite(params.max_variance) || !std::isfinite(params.innovation_gate_sigma))
            {
                // Keep default parameters if validation fails
                return;
            }

            // Validate parameter ranges
            if (params.min_variance < 0.0 || params.max_variance < params.min_variance ||
                params.max_abs_position < 0.0 || params.max_abs_velocity < 0.0 || params.innovation_gate_sigma < 0.0)
            {
                // Keep default parameters if range validation fails
                return;
            }

            params_ = params;
            clamp_covariance();
            x_ = clamp_position(x_);
            v_ = clamp_velocity(v_);
        }

        void reset(double position = 0.0, double velocity = 0.0) noexcept;

        bool update(double position_measurement, double assumed_accel_mps2 = 0.0) noexcept;

        // Timestamped update: computes dt from consecutive timestamps
        bool update(double position_measurement, double assumed_accel_mps2, std::uint64_t timestamp_ns) noexcept;

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
        [[nodiscard]] std::uint64_t rejected_count() const noexcept
        {
            return rejected_count_;
        }

      private:
        [[nodiscard]] double dt_seconds() const noexcept;
        void predict(double assumed_accel_mps2, double dt) noexcept;
        void clamp_covariance() noexcept;
        [[nodiscard]] double clamp_position(double value) const noexcept;
        [[nodiscard]] double clamp_velocity(double value) const noexcept;
        [[nodiscard]] bool innovation_gate(double innovation, double s) const noexcept;

        const config::SystemConfig* config_{nullptr};
        BoundedEkfParams params_{};

        double x_{0.0};
        double v_{0.0};

        double p00_{1.0};
        double p01_{0.0};
        double p10_{0.0};
        double p11_{1.0};

        bool healthy_{true};
        std::uint64_t last_timestamp_ns_{0U};
        std::uint64_t rejected_count_{0U};
    };

} // namespace safety_core::filters
