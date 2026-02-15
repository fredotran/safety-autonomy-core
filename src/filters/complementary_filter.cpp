#include "safety_core/filters/complementary_filter.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace safety_core::filters
{

    double ComplementaryFilter::update(double rate_measurement, double absolute_measurement) noexcept
    {
        const double dt            = dt_seconds();
        const double predicted     = state_ + (rate_measurement * dt);
        const double alpha         = std::clamp(params_.alpha, 0.0, 1.0);
        const double complementary = (alpha * predicted) + ((1.0 - alpha) * absolute_measurement);
        state_                     = clamp_finite(complementary);
        return state_;
    }

    double ComplementaryFilter::dt_seconds() const noexcept
    {
        if ((config_ != nullptr) && (config_->timing.control_period.count() > 0))
        {
            return std::chrono::duration<double>(config_->timing.control_period).count();
        }
        return 0.1;
    }

    double ComplementaryFilter::clamp_finite(double value) const noexcept
    {
        if (!std::isfinite(value))
        {
            return 0.0;
        }
        const double bound = std::max(0.0, params_.max_abs_state);
        return std::clamp(value, -bound, bound);
    }

} // namespace safety_core::filters
