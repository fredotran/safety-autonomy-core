#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/config/system_config.hpp"

#include <cstdint>

namespace safety_core::filters
{

    // Commentary: Complementary filter keeps bounded, low-cost fusion for rate + absolute measurements.
    struct ComplementaryFilterParams
    {
        double alpha{0.98};
        double max_abs_state{1000.0};
        double cutoff_frequency_hz{0.0}; // If > 0, alpha is derived from cutoff + dt
    };

    class ComplementaryFilter
    {
      public:
        void apply_config(const config::SystemConfig& cfg) noexcept
        {
            config_ = &cfg;
        }

        void set_params(const ComplementaryFilterParams& params) noexcept
        {
            params_ = params;
        }

        void reset(double state = 0.0) noexcept
        {
            state_             = clamp_finite(state);
            last_timestamp_ns_ = 0U;
        }

        [[nodiscard]] double state() const noexcept
        {
            return state_;
        }

        double update(double rate_measurement, double absolute_measurement) noexcept;

        // Timestamped update: computes dt from consecutive timestamps
        double update(double rate_measurement, double absolute_measurement, std::uint64_t timestamp_ns) noexcept;

      private:
        [[nodiscard]] double dt_seconds() const noexcept;
        [[nodiscard]] double compute_alpha(double dt) const noexcept;
        [[nodiscard]] double clamp_finite(double value) const noexcept;

        const config::SystemConfig* config_{nullptr};
        ComplementaryFilterParams params_{};
        double state_{0.0};
        std::uint64_t last_timestamp_ns_{0U};
    };

} // namespace safety_core::filters
