#include "safety_core/control/safety_pid.hpp"

#include "safety_core/common/time.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/topics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace safety_core::control
{
    namespace
    {
        constexpr double kDefaultDtSeconds = 0.1;
        static_assert(96U < diag::kDiagnosticPayloadCapacity);
        static_assert(160U < diag::kDiagnosticPayloadCapacity);
    } // namespace

    SafetyPidController::SafetyPidController(platform::Clock* clock) noexcept : clock_(clock)
    {
        reset();
        last_localization_update_ = now();
    }

    void SafetyPidController::set_clock(platform::Clock* clock) noexcept
    {
        clock_ = clock;
    }

    void SafetyPidController::set_diagnostic_transport(diag::DiagnosticTransport* transport) noexcept
    {
        diag_transport_ = transport;
    }

    Result SafetyPidController::apply_config(const config::SystemConfig& cfg) noexcept
    {
        config_ = &cfg;
        reset();
        if (cfg.envelope.max_speed_mps <= 0.0 || cfg.envelope.max_accel_mps2 <= 0.0)
        {
            return Result::Fault("invalid motion envelope config");
        }
        return Result::Ok();
    }

    void SafetyPidController::set_gains(const PidGains& gains) noexcept
    {
        gains_ = gains;
    }

    void SafetyPidController::reset() noexcept
    {
        integral_     = 0.0;
        prev_error_   = 0.0;
        last_output_  = 0.0;
        first_update_ = true;
        last_update_  = now();
    }

    void SafetyPidController::mark_localization_update(time::TimePoint stamp) noexcept
    {
        last_localization_update_ = stamp;
    }

    double SafetyPidController::compute(double setpoint, double measurement) noexcept
    {
        const time::TimePoint now_tp = now();
        const bool loc_valid         = localization_valid(now_tp);
        if (!loc_valid)
        {
            reset();
            last_localization_update_ = now_tp;
            char payload[96]{};
            static_cast<void>(std::snprintf(payload, sizeof(payload), "setpoint=%.6f", setpoint));
            publish_event(diag::topic::kPidLocalizationStale.data(), std::string_view(payload));
            return 0.0;
        }

        double dt = kDefaultDtSeconds;
        if (config_ != nullptr)
        {
            const double cfg_dt = std::chrono::duration<double>(config_->timing.control_period).count();
            if (cfg_dt > 0.0)
            {
                dt = cfg_dt;
            }
        }

        if (!first_update_)
        {
            const double elapsed = std::chrono::duration<double>(now_tp - last_update_).count();
            if (elapsed > 1e-6)
            {
                dt = elapsed;
            }
        }

        const double error = setpoint - measurement;
        integral_ += error * dt;

        // Anti-windup: clamp integral to prevent accumulation during saturation
        if (gains_.max_integral > 0.0)
        {
            integral_ = std::clamp(integral_, -gains_.max_integral, gains_.max_integral);
        }

        double derivative = 0.0;
        if (!first_update_ && dt > 0.0)
        {
            derivative = (error - prev_error_) / dt;
        }

        double output = (gains_.kp * error) + (gains_.ki * integral_) + (gains_.kd * derivative);
        output        = clamp_speed(output);

        // Back-calculation anti-windup: undo integration that would cause saturation
        const double pre_accel_output = output;
        output                        = clamp_accel(output, dt);
        if (std::abs(pre_accel_output - output) > 1e-9 && gains_.ki != 0.0)
        {
            integral_ -= (pre_accel_output - output) / gains_.ki;
        }

        prev_error_   = error;
        last_output_  = output;
        last_update_  = now_tp;
        first_update_ = false;

        char payload[160]{};
        static_cast<void>(std::snprintf(payload, sizeof(payload), "setpoint=%.6f measurement=%.6f output=%.6f",
                                        setpoint, measurement, output));
        publish_event(diag::topic::kPidOutput.data(), std::string_view(payload));

        return output;
    }

    time::TimePoint SafetyPidController::now() const noexcept
    {
        return (clock_ != nullptr) ? clock_->now() : time::now();
    }

    bool SafetyPidController::localization_valid(time::TimePoint stamp) const noexcept
    {
        if (config_ == nullptr)
        {
            return true;
        }
        const auto timeout = config_->timing.localization_timeout;
        if (timeout.count() == 0)
        {
            return true;
        }
        return (stamp - last_localization_update_) <= timeout;
    }

    double SafetyPidController::clamp_speed(double value) const noexcept
    {
        if ((config_ == nullptr) || (config_->envelope.max_speed_mps <= 0.0))
        {
            return value;
        }
        const double limit = config_->envelope.max_speed_mps;
        return std::clamp(value, -limit, limit);
    }

    double SafetyPidController::clamp_accel(double desired, double dt) const noexcept
    {
        if ((config_ == nullptr) || (config_->envelope.max_accel_mps2 <= 0.0) || (dt <= 0.0))
        {
            return desired;
        }
        const double max_delta = config_->envelope.max_accel_mps2 * dt;
        const double delta     = desired - last_output_;
        double limited         = desired;
        if (delta > max_delta)
        {
            limited = last_output_ + max_delta;
        }
        else if (delta < -max_delta)
        {
            limited = last_output_ - max_delta;
        }
        return clamp_speed(limited);
    }

    double SafetyPidController::compute(double setpoint, double measurement, double ff_velocity,
                                        double ff_acceleration) noexcept
    {
        const double pid_output = compute(setpoint, measurement);
        if (!std::isfinite(ff_velocity) || !std::isfinite(ff_acceleration))
        {
            return pid_output;
        }

        double dt = kDefaultDtSeconds;
        if (config_ != nullptr)
        {
            const double cfg_dt = std::chrono::duration<double>(config_->timing.control_period).count();
            if (cfg_dt > 0.0)
            {
                dt = cfg_dt;
            }
        }

        double output = pid_output + ff_velocity + (ff_acceleration * dt);
        output        = clamp_speed(output);
        last_output_  = output;
        return output;
    }

    void SafetyPidController::publish_event(const char* topic, std::string_view payload) const noexcept
    {
        if (diag_transport_ == nullptr)
        {
            return;
        }

        diag::DiagnosticEvent event{};
        event.set_topic(topic);
        event.set_payload(payload);
        event.timestamp_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now().time_since_epoch()).count());
        diag_transport_->publish(event);
    }

} // namespace safety_core::control
