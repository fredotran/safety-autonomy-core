#include "safety_core/filters/bounded_ekf_filter.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace safety_core::filters
{

    void BoundedEkfFilter::reset(double position, double velocity) noexcept
    {
        x_ = clamp_position(position);
        v_ = clamp_velocity(velocity);

        p00_ = 1.0;
        p01_ = 0.0;
        p10_ = 0.0;
        p11_ = 1.0;
        clamp_covariance();
        healthy_           = true;
        last_timestamp_ns_ = 0U;
        rejected_count_    = 0U;
    }

    bool BoundedEkfFilter::update(double position_measurement, double assumed_accel_mps2) noexcept
    {
        if (!std::isfinite(position_measurement) || !std::isfinite(assumed_accel_mps2))
        {
            healthy_ = false;
            return false;
        }

        const double dt = dt_seconds();
        predict(assumed_accel_mps2, dt);

        const double r = std::max(params_.measurement_noise, params_.min_variance);
        const double s = p00_ + r;
        if (!std::isfinite(s) || (s <= params_.min_variance))
        {
            healthy_ = false;
            return false;
        }

        const double innovation = position_measurement - x_;

        // Innovation gating: reject outliers via Mahalanobis distance
        if (!innovation_gate(innovation, s))
        {
            ++rejected_count_;
            return healthy_;
        }

        const double k0 = p00_ / s;
        const double k1 = p10_ / s;

        const double old_p00 = p00_;
        const double old_p01 = p01_;
        const double old_p10 = p10_;
        const double old_p11 = p11_;

        x_ += k0 * innovation;
        v_ += k1 * innovation;

        p00_ = (1.0 - k0) * old_p00;
        p01_ = (1.0 - k0) * old_p01;
        p10_ = old_p10 - (k1 * old_p00);
        p11_ = old_p11 - (k1 * old_p01);

        x_ = clamp_position(x_);
        v_ = clamp_velocity(v_);
        clamp_covariance();

        healthy_ = std::isfinite(x_) && std::isfinite(v_);
        return healthy_;
    }

    bool BoundedEkfFilter::update(double position_measurement, double assumed_accel_mps2,
                                  std::uint64_t timestamp_ns) noexcept
    {
        if (!std::isfinite(position_measurement) || !std::isfinite(assumed_accel_mps2))
        {
            healthy_ = false;
            return false;
        }

        // Compute dt from timestamps
        double dt = dt_seconds();
        if (last_timestamp_ns_ > 0U && timestamp_ns > last_timestamp_ns_)
        {
            dt = static_cast<double>(timestamp_ns - last_timestamp_ns_) * 1e-9;
        }
        last_timestamp_ns_ = timestamp_ns;

        // Reject stale measurements (> 10x expected period)
        const double expected_dt = dt_seconds();
        if (dt > (expected_dt * 10.0))
        {
            return healthy_;
        }

        predict(assumed_accel_mps2, dt);

        const double r = std::max(params_.measurement_noise, params_.min_variance);
        const double s = p00_ + r;
        if (!std::isfinite(s) || (s <= params_.min_variance))
        {
            healthy_ = false;
            return false;
        }

        const double innovation = position_measurement - x_;

        if (!innovation_gate(innovation, s))
        {
            ++rejected_count_;
            return healthy_;
        }

        const double k0 = p00_ / s;
        const double k1 = p10_ / s;

        const double old_p00 = p00_;
        const double old_p01 = p01_;
        const double old_p10 = p10_;
        const double old_p11 = p11_;

        x_ += k0 * innovation;
        v_ += k1 * innovation;

        p00_ = (1.0 - k0) * old_p00;
        p01_ = (1.0 - k0) * old_p01;
        p10_ = old_p10 - (k1 * old_p00);
        p11_ = old_p11 - (k1 * old_p01);

        x_ = clamp_position(x_);
        v_ = clamp_velocity(v_);
        clamp_covariance();

        healthy_ = std::isfinite(x_) && std::isfinite(v_);
        return healthy_;
    }

    double BoundedEkfFilter::dt_seconds() const noexcept
    {
        if ((config_ != nullptr) && (config_->timing.control_period.count() > 0))
        {
            return std::chrono::duration<double>(config_->timing.control_period).count();
        }
        return 0.1;
    }

    void BoundedEkfFilter::predict(double assumed_accel_mps2, double dt) noexcept
    {
        x_ += (v_ * dt) + (0.5 * assumed_accel_mps2 * dt * dt);
        v_ += assumed_accel_mps2 * dt;

        const double old_p00 = p00_;
        const double old_p01 = p01_;
        const double old_p10 = p10_;
        const double old_p11 = p11_;

        const double q_pos = std::max(params_.process_noise_position, params_.min_variance);
        const double q_vel = std::max(params_.process_noise_velocity, params_.min_variance);

        p00_ = old_p00 + (dt * (old_p10 + old_p01)) + (dt * dt * old_p11) + q_pos;
        p01_ = old_p01 + (dt * old_p11);
        p10_ = old_p10 + (dt * old_p11);
        p11_ = old_p11 + q_vel;

        x_ = clamp_position(x_);
        v_ = clamp_velocity(v_);
        clamp_covariance();
    }

    bool BoundedEkfFilter::innovation_gate(double innovation, double s) const noexcept
    {
        if (params_.innovation_gate_sigma <= 0.0)
        {
            return true; // gating disabled
        }
        // Mahalanobis distance for 1D: innovation^2 / S
        const double mahal_sq  = (innovation * innovation) / s;
        const double threshold = params_.innovation_gate_sigma * params_.innovation_gate_sigma;
        return mahal_sq <= threshold;
    }

    void BoundedEkfFilter::clamp_covariance() noexcept
    {
        const double min_v = std::max(params_.min_variance, 0.0);
        const double max_v = std::max(params_.max_variance, min_v);

        if (!std::isfinite(p00_))
        {
            p00_ = max_v;
        }
        if (!std::isfinite(p11_))
        {
            p11_ = max_v;
        }
        if (!std::isfinite(p01_))
        {
            p01_ = 0.0;
        }
        if (!std::isfinite(p10_))
        {
            p10_ = 0.0;
        }

        p00_ = std::clamp(p00_, min_v, max_v);
        p11_ = std::clamp(p11_, min_v, max_v);

        const double cross_bound = max_v;
        p01_                     = std::clamp(p01_, -cross_bound, cross_bound);
        p10_                     = std::clamp(p10_, -cross_bound, cross_bound);
    }

    double BoundedEkfFilter::clamp_position(double value) const noexcept
    {
        if (!std::isfinite(value))
        {
            return 0.0;
        }
        const double bound = std::max(0.0, params_.max_abs_position);
        return std::clamp(value, -bound, bound);
    }

    double BoundedEkfFilter::clamp_velocity(double value) const noexcept
    {
        if (!std::isfinite(value))
        {
            return 0.0;
        }
        const double bound = std::max(0.0, params_.max_abs_velocity);
        return std::clamp(value, -bound, bound);
    }

} // namespace safety_core::filters
