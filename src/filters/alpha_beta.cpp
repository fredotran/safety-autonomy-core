// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/filters/alpha_beta_filter.hpp"

#include <chrono>
#include <cmath>

namespace safety_core::filters
{

    double AlphaBetaFilter::update(double measurement) noexcept
    {
        if (!std::isfinite(measurement))
        {
            return position_;
        }
        const double dt         = dt_seconds();
        const double prediction = position_ + (velocity_ * dt);
        const double residual   = measurement - prediction;
        position_               = prediction + (params_.alpha * residual);
        velocity_               = velocity_ + ((params_.beta / dt) * residual);
        recover_if_diverged();
        return position_;
    }

    double AlphaBetaFilter::update(double measurement, std::uint64_t timestamp_ns) noexcept
    {
        if (!std::isfinite(measurement))
        {
            return position_;
        }
        double dt = dt_seconds();
        if (last_timestamp_ns_ > 0U && timestamp_ns > last_timestamp_ns_)
        {
            dt = static_cast<double>(timestamp_ns - last_timestamp_ns_) * 1e-9;
        }
        last_timestamp_ns_ = timestamp_ns;

        if (dt <= 0.0)
        {
            dt = dt_seconds();
        }

        const double prediction = position_ + (velocity_ * dt);
        const double residual   = measurement - prediction;
        position_               = prediction + (params_.alpha * residual);
        velocity_               = velocity_ + ((params_.beta / dt) * residual);
        recover_if_diverged();
        return position_;
    }

    void AlphaBetaFilter::recover_if_diverged() noexcept
    {
        // A single bad measurement (or an absurd dt) can push the state to
        // NaN/Inf. Without recovery the state stays NaN forever — reset to a
        // known-safe origin so the next valid sample restarts cleanly.
        if (!std::isfinite(position_) || !std::isfinite(velocity_))
        {
            position_ = 0.0;
            velocity_ = 0.0;
        }
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
