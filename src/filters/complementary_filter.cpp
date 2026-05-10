// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/filters/complementary_filter.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace safety_core::filters
{

    namespace
    {
        constexpr double kTwoPi = 6.283185307179586;
    }

    double ComplementaryFilter::update(double rate_measurement, double absolute_measurement) noexcept
    {
        const double dt            = dt_seconds();
        const double predicted     = state_ + (rate_measurement * dt);
        const double alpha         = compute_alpha(dt);
        const double complementary = (alpha * predicted) + ((1.0 - alpha) * absolute_measurement);
        state_                     = clamp_finite(complementary);
        return state_;
    }

    double ComplementaryFilter::update(double rate_measurement, double absolute_measurement,
                                       std::uint64_t timestamp_ns) noexcept
    {
        double dt = dt_seconds();
        if (last_timestamp_ns_ > 0U && timestamp_ns > last_timestamp_ns_)
        {
            dt = static_cast<double>(timestamp_ns - last_timestamp_ns_) * 1e-9;
        }
        last_timestamp_ns_ = timestamp_ns;

        const double predicted     = state_ + (rate_measurement * dt);
        const double alpha         = compute_alpha(dt);
        const double complementary = (alpha * predicted) + ((1.0 - alpha) * absolute_measurement);
        state_                     = clamp_finite(complementary);
        return state_;
    }

    double ComplementaryFilter::compute_alpha(double dt) const noexcept
    {
        if (params_.cutoff_frequency_hz > 0.0 && dt > 0.0)
        {
            // Derive alpha from cutoff frequency: tau = 1/(2*pi*f), alpha = tau/(tau+dt)
            const double tau = 1.0 / (kTwoPi * params_.cutoff_frequency_hz);
            return std::clamp(tau / (tau + dt), 0.0, 1.0);
        }
        return std::clamp(params_.alpha, 0.0, 1.0);
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
