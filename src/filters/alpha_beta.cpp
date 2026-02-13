#include "safety_core/filters/alpha_beta_filter.hpp"

#include <chrono>

namespace safety_core::filters
{

    double AlphaBetaFilter::update(double measurement) noexcept
    {
        const double dt         = dt_seconds();
        const double prediction = position_ + (velocity_ * dt);
        const double residual   = measurement - prediction;
        position_               = prediction + (params_.alpha * residual);
        velocity_               = velocity_ + ((params_.beta / dt) * residual);
        return position_;
    }

    double AlphaBetaFilter::dt_seconds() const noexcept
    {
        if ((config_ != nullptr) && (config_->timing.control_period.count() > 0))
        {
            return std::chrono::duration<double>(config_->timing.control_period).count();
        }
        return 0.1; // default 100 ms loop
    }

} // namespace safety_core::filters
