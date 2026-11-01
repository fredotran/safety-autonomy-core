// @req SR-FILTER-001
// Deterministic property-based tests: a fixed-seed PRNG drives randomized
// inputs through the safety boundaries and asserts the invariants that must
// hold for every input — finite outputs, bounded commands, monotonic zone
// severity, and crash-free parsing.
//
// The fixed seed keeps runs reproducible; bump kIterations or the seed in CI
// jobs rather than here.

#include "safety_core/config/env_loader.hpp"
#include "safety_core/control/safety_pid.hpp"
#include "safety_core/filters/alpha_beta_filter.hpp"
#include "safety_core/filters/complementary_filter.hpp"
#include "safety_core/platform/manual_clock.hpp"
#include "safety_core/safety/safety_envelope.hpp"
#include "test_support.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>

namespace
{
    using safety_core::test_support::check;

    constexpr std::uint64_t kSeed     = 0x5AFE2026ULL;
    constexpr std::size_t kIterations = 20000U;

    // Uniform real in [lo, hi]; a small fraction of draws return NaN/Inf to
    // exercise the non-finite guards on every boundary.
    class InputGen
    {
      public:
        InputGen() : engine_(kSeed) {}

        double real(double lo, double hi, bool allow_nonfinite = true)
        {
            if (allow_nonfinite)
            {
                const std::uint32_t pick = u32() % 50U;
                if (pick == 0U)
                {
                    return std::numeric_limits<double>::quiet_NaN();
                }
                if (pick == 1U)
                {
                    return std::numeric_limits<double>::infinity();
                }
                if (pick == 2U)
                {
                    return -std::numeric_limits<double>::infinity();
                }
            }
            std::uniform_real_distribution<double> dist(lo, hi);
            return dist(engine_);
        }

        std::uint64_t u64(std::uint64_t lo, std::uint64_t hi)
        {
            std::uniform_int_distribution<std::uint64_t> dist(lo, hi);
            return dist(engine_);
        }

      private:
        std::uint32_t u32()
        {
            std::uniform_int_distribution<std::uint32_t> dist(0U, 0xFFFFFFFFU);
            return dist(engine_);
        }

        std::mt19937_64 engine_;
    };

    int zone_rank(safety_core::safety::SafetyZone zone)
    {
        switch (zone)
        {
        case safety_core::safety::SafetyZone::Clear:
            return 0;
        case safety_core::safety::SafetyZone::Warning:
            return 1;
        case safety_core::safety::SafetyZone::Protective:
            return 2;
        case safety_core::safety::SafetyZone::Emergency:
            return 3;
        }
        return -1;
    }
} // namespace

int main()
{
    using namespace std::chrono_literals;
    using safety_core::safety::evaluate_stop_distance;
    using safety_core::safety::SafetyZone;

    bool ok = true;
    InputGen gen;

    // ------------------------------------------------------------------
    // Envelope: zone severity is monotonic non-decreasing as distance
    // shrinks, and every evaluation yields a valid zone.
    // ------------------------------------------------------------------
    for (std::size_t i = 0U; i < kIterations; ++i)
    {
        const double speed     = gen.real(0.0, 5.0, false);
        const double far_dist  = gen.real(0.0, 20.0, false);
        const double near_dist = far_dist * gen.real(0.0, 1.0, false);

        const auto near_eval = evaluate_stop_distance(near_dist, speed, 0.8, 0.1, 0.1);
        const auto far_eval  = evaluate_stop_distance(far_dist, speed, 0.8, 0.1, 0.1);

        ok &= check(zone_rank(near_eval.zone) >= 0 && zone_rank(far_eval.zone) >= 0,
                    "zone must always be a valid enum value");
        ok &= check(zone_rank(near_eval.zone) >= zone_rank(far_eval.zone),
                    "closer obstacle must not classify less severe");
        ok &= check(!near_eval.within_envelope || far_eval.within_envelope ||
                        zone_rank(far_eval.zone) <= zone_rank(near_eval.zone),
                    "envelope breach ordering must be consistent");
        if (!ok)
        {
            std::cerr << "  near=" << near_dist << " far=" << far_dist << " speed=" << speed << '\n';
            return 1;
        }
    }

    // ------------------------------------------------------------------
    // PID: for finite inputs the command output is always finite and
    // bounded by the configured speed limit; non-finite inputs command 0.
    // ------------------------------------------------------------------
    {
        safety_core::control::SafetyPidController pid;
        safety_core::platform::ManualClock clock;
        pid.set_clock(&clock);
        pid.set_gains(safety_core::control::PidGains{1.0, 0.5, 0.1, 50.0});

        safety_core::config::SystemConfig cfg = safety_core::test_support::make_motion_defaults();
        static_cast<void>(pid.apply_config(cfg));
        pid.mark_localization_update(clock.now());

        for (std::size_t i = 0U; i < kIterations; ++i)
        {
            clock.advance(std::chrono::milliseconds{static_cast<long long>(gen.u64(1U, 200U))});
            pid.mark_localization_update(clock.now());

            const double setpoint    = gen.real(-5.0, 5.0);
            const double measurement = gen.real(-10.0, 10.0);
            const double output      = pid.compute(setpoint, measurement);

            if (!std::isfinite(setpoint) || !std::isfinite(measurement))
            {
                ok &= check(output == 0.0, "non-finite PID input must command zero");
            }
            else
            {
                ok &= check(std::isfinite(output), "PID output must stay finite for finite inputs");
                ok &= check(std::abs(output) <= cfg.envelope.max_speed_mps + 1e-9,
                            "PID output must respect the configured speed limit");
            }

            // Feedforward overload must also bound its output.
            const double ff_v   = gen.real(-3.0, 3.0);
            const double ff_a   = gen.real(-2.0, 2.0);
            const double ff_out = pid.compute(setpoint, measurement, ff_v, ff_a);
            ok &= check(std::isfinite(ff_out), "feedforward output must stay finite");
            ok &= check(std::abs(ff_out) <= cfg.envelope.max_speed_mps + 1e-9,
                        "feedforward output must respect the speed limit");

            if (!ok)
            {
                std::cerr << "  setpoint=" << setpoint << " measurement=" << measurement << " output=" << output
                          << '\n';
                return 1;
            }
        }
    }

    // ------------------------------------------------------------------
    // Filters: complementary state is always finite (clamp); alpha-beta
    // rejects non-finite samples and recovers from divergence.
    // ------------------------------------------------------------------
    {
        safety_core::filters::AlphaBetaFilter alpha_beta;
        safety_core::filters::ComplementaryFilter complementary;
        complementary.set_params(safety_core::filters::ComplementaryFilterParams{0.9, 1e6, 0.0});

        std::uint64_t ts = 0U;
        for (std::size_t i = 0U; i < kIterations; ++i)
        {
            const double measurement = gen.real(-100.0, 100.0);
            // Timestamps mostly increase, sometimes regress or stall.
            if ((i % 7U) != 0U)
            {
                ts += gen.u64(0U, 500000000ULL);
            }
            else
            {
                ts = gen.u64(0U, ts);
            }

            const double ab   = alpha_beta.update(measurement, ts);
            const double comp = complementary.update(measurement, measurement, ts);
            ok &= check(std::isfinite(ab), "alpha-beta state must stay finite");
            ok &= check(std::isfinite(comp), "complementary state must stay finite");
            if (!ok)
            {
                std::cerr << "  i=" << i << " measurement=" << measurement << " ts=" << ts << " ab=" << ab
                          << " comp=" << comp << '\n';
                return 1;
            }
        }
    }

    // ------------------------------------------------------------------
    // env_loader: arbitrary strings never crash and every parsed field
    // stays finite (parser rejects non-finite).
    // ------------------------------------------------------------------
    {
        using safety_core::test_support::clear_config_env_vars;
        constexpr const char* kVars[] = {
            "SAFETY_CORE_MAX_TASKS",      "SAFETY_CORE_MAX_SPEED_MPS",     "SAFETY_CORE_MAX_ACCEL_MPS2",
            "SAFETY_CORE_MAX_DECEL_MPS2", "SAFETY_CORE_CONTROL_LATENCY_S", "SAFETY_CORE_SAFETY_BUFFER_M",
            "SAFETY_CORE_CONFIG_VERSION",
        };
        constexpr const char* kFragments[] = {
            "1",
            "-1",
            "+1",
            " 1",
            "1 ",
            "1.5",
            "1e3",
            "1e-3",
            "0x10",
            "08",
            "nan",
            "NaN",
            "inf",
            "-inf",
            "infinity",
            "abc",
            "",
            " ",
            "--1",
            "1e999",
            "-1e999",
            "18446744073709551616",
            "99999999999999999999",
            "0",
            "255",
            "256",
            "1.5.5",
            ".5",
            "5.",
            "++--",
            "0b101",
        };

        for (std::size_t i = 0U; i < 4000U; ++i)
        {
            clear_config_env_vars();
            // Assign 1-3 random variables per iteration.
            const std::size_t count = 1U + static_cast<std::size_t>(gen.u64(0U, 2U));
            for (std::size_t j = 0U; j < count; ++j)
            {
                const char* var  = kVars[gen.u64(0U, (sizeof(kVars) / sizeof(kVars[0])) - 1U)];
                const char* frag = kFragments[gen.u64(0U, (sizeof(kFragments) / sizeof(kFragments[0])) - 1U)];
                // Occasionally concatenate two fragments for extra nastiness.
                std::string value = frag;
                if ((i % 5U) == 0U)
                {
                    value += kFragments[gen.u64(0U, (sizeof(kFragments) / sizeof(kFragments[0])) - 1U)];
                }
                setenv(var, value.c_str(), 1);
            }

            const safety_core::config::SystemConfig cfg = safety_core::config::load_from_env();
            ok &=
                check(std::isfinite(cfg.envelope.max_speed_mps) && std::isfinite(cfg.envelope.max_accel_mps2) &&
                          std::isfinite(cfg.envelope.max_comfort_decel_mps2) &&
                          std::isfinite(cfg.envelope.control_latency_s) && std::isfinite(cfg.envelope.safety_buffer_m),
                      "env parser must never emit non-finite fields");
            if (!ok)
            {
                return 1;
            }
        }
        clear_config_env_vars();
    }

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] property tests (" << kIterations << " iterations, seed " << kSeed << ")" << '\n';
    return 0;
}
