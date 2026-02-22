#include "safety_core/config/system_config.hpp"
#include "safety_core/filters/bounded_ekf_filter.hpp"
#include "safety_core/filters/complementary_filter.hpp"
#include "test_support.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    using safety_core::test_support::check;

    safety_core::config::SystemConfig make_defaults()
    {
        using namespace std::chrono_literals;

        safety_core::config::SystemConfig cfg{};
        cfg.config_version                  = safety_core::config::kCurrentSystemConfigVersion;
        cfg.timing.control_period           = 100ms;
        cfg.timing.watchdog_period          = 200ms;
        cfg.timing.localization_timeout     = 2s;
        cfg.envelope.max_speed_mps          = 2.0;
        cfg.envelope.max_accel_mps2         = 1.0;
        cfg.envelope.max_comfort_decel_mps2 = 1.0;
        cfg.envelope.control_latency_s      = 0.1;
        cfg.envelope.safety_buffer_m        = 0.2;
        cfg.max_tasks                       = 4U;
        return cfg;
    }

} // namespace

int main()
{
    using safety_core::filters::BoundedEkfFilter;
    using safety_core::filters::BoundedEkfParams;
    using safety_core::filters::ComplementaryFilter;
    using safety_core::filters::ComplementaryFilterParams;

    bool ok        = true;
    const auto cfg = make_defaults();

    // Commentary: Bounded EKF property sweep checks finite bounded state under bounded input noise.
    {
        BoundedEkfFilter ekf;
        ekf.apply_config(cfg);
        BoundedEkfParams params{};
        params.max_abs_position = 5.0;
        params.max_abs_velocity = 2.0;
        params.max_variance     = 10.0;
        ekf.set_params(params);
        ekf.reset(1000.0, 1000.0);

        ok &=
            check(std::abs(ekf.position()) <= params.max_abs_position + 1e-9, "EKF reset should clamp position bounds");
        ok &=
            check(std::abs(ekf.velocity()) <= params.max_abs_velocity + 1e-9, "EKF reset should clamp velocity bounds");

        for (int i = 0; i < 200; ++i)
        {
            const double measurement = std::sin(static_cast<double>(i) * 0.1) * 5.0;
            const bool healthy       = ekf.update(measurement, 0.0);
            ok &= check(healthy, "EKF should remain healthy for finite bounded measurement input");
            ok &= check(std::isfinite(ekf.position()), "EKF position must stay finite");
            ok &= check(std::isfinite(ekf.velocity()), "EKF velocity must stay finite");
            ok &= check(std::abs(ekf.position()) <= params.max_abs_position + 1e-9,
                        "EKF position should remain within configured bounds");
            ok &= check(std::abs(ekf.velocity()) <= params.max_abs_velocity + 1e-9,
                        "EKF velocity should remain within configured bounds");
        }

        ok &= check(!ekf.update(NAN, 0.0), "EKF should reject non-finite measurement");
        ok &= check(!ekf.update(0.0, std::numeric_limits<double>::infinity()),
                    "EKF should reject non-finite acceleration input");
    }

    {
        BoundedEkfFilter ekf;
        ekf.apply_config(cfg);
        BoundedEkfParams params{};
        params.max_abs_position       = 20.0;
        params.max_abs_velocity       = 5.0;
        params.process_noise_position = 1e5;
        params.process_noise_velocity = 1e5;
        params.max_variance           = 1.0;
        ekf.set_params(params);
        ekf.reset(0.0, 0.0);

        for (int i = 0; i < 100; ++i)
        {
            const bool healthy = ekf.update(0.1 * static_cast<double>(i), 50.0);
            ok &= check(healthy, "EKF should remain healthy under large but finite stimuli");
            ok &= check(std::abs(ekf.position()) <= params.max_abs_position + 1e-9,
                        "EKF position should stay clamped under stress");
            ok &= check(std::abs(ekf.velocity()) <= params.max_abs_velocity + 1e-9,
                        "EKF velocity should stay clamped under stress");
        }
    }

    {
        ComplementaryFilter filter;
        filter.apply_config(cfg);
        ComplementaryFilterParams params{};
        params.alpha         = 0.9;
        params.max_abs_state = 20.0;
        filter.set_params(params);
        filter.reset(0.0);

        double previous = filter.state();
        for (int i = 0; i < 120; ++i)
        {
            const double fused = filter.update(0.1, 1.0);
            ok &= check(std::isfinite(fused), "complementary filter output should be finite");
            ok &= check(std::abs(fused) <= params.max_abs_state + 1e-9,
                        "complementary filter output should stay bounded");
            ok &= check(fused >= previous - 1e-6,
                        "complementary filter should converge monotonically for constant positive input");
            previous = fused;
        }
    }

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] filter invariants tests" << '\n';
    return 0;
}
