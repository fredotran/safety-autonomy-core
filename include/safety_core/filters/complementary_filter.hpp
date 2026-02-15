#pragma once

#include "safety_core/config/system_config.hpp"

namespace safety_core::filters
{

    // Commentary: Complementary filter keeps bounded, low-cost fusion for rate + absolute measurements.
    struct ComplementaryFilterParams
    {
        double alpha{0.98};
        double max_abs_state{1000.0};
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
            state_ = clamp_finite(state);
        }

        [[nodiscard]] double state() const noexcept
        {
            return state_;
        }

        double update(double rate_measurement, double absolute_measurement) noexcept;

      private:
        [[nodiscard]] double dt_seconds() const noexcept;
        [[nodiscard]] double clamp_finite(double value) const noexcept;

        const config::SystemConfig* config_{nullptr};
        ComplementaryFilterParams params_{};
        double state_{0.0};
    };

} // namespace safety_core::filters
