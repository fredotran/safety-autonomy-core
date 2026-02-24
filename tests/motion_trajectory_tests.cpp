#include "safety_core/motion/trajectory.hpp"
#include "test_support.hpp"

#include <iostream>
#include <limits>

namespace
{
    using safety_core::test_support::check;
    using safety_core::test_support::make_motion_defaults;

} // namespace

int main()
{
    using safety_core::motion::generate_emergency_stop_profile;
    using safety_core::motion::TrajectoryPoint;
    using safety_core::motion::TrajectoryValidator;
    using safety_core::motion::TrajectoryViolation;

    bool ok = true;

    TrajectoryPoint points[128]{};
    std::size_t count = 0U;

    ok &= check(generate_emergency_stop_profile(1.5, 0.8, 0.1, points, 128U, count),
                "emergency stop primitive generation should succeed");
    ok &= check(count > 1U, "emergency stop primitive should produce multiple points");
    ok &= check(points[count - 1U].speed_mps == 0.0, "emergency stop profile should end at zero speed");

    // Commentary: validator enforces monotonic timeline and bounded kinematics from system config.
    TrajectoryValidator validator;
    const auto cfg = make_motion_defaults();
    validator.apply_config(cfg);
    validator.set_max_jerk_mps3(4.0);

    auto valid_result = validator.validate(points, count);
    ok &= check(valid_result.ok, "generated emergency stop trajectory should validate");

    validator.set_obstacle_distance_m(5.0);
    const auto envelope_ok = validator.validate(points, count);
    ok &= check(envelope_ok.ok, "trajectory should remain within stopping envelope for distant obstacle");

    validator.set_obstacle_distance_m(0.1);
    const auto envelope_fail = validator.validate(points, count);
    ok &= check(!envelope_fail.ok && envelope_fail.violation == TrajectoryViolation::EnvelopeViolation,
                "trajectory should fail envelope check for near obstacle");
    ok &= check(envelope_fail.failing_index == 0U, "envelope failure should report first violating point");
    validator.set_obstacle_distance_m(-1.0);

    const auto null_fail = validator.validate(nullptr, count);
    ok &= check(!null_fail.ok && null_fail.violation == TrajectoryViolation::NullInput,
                "validator should reject null trajectory buffer");
    ok &= check(null_fail.failing_index == 0U, "null input failure index should be zero");

    const auto empty_fail = validator.validate(points, 0U);
    ok &= check(!empty_fail.ok && empty_fail.violation == TrajectoryViolation::EmptyTrajectory,
                "validator should reject empty trajectory");
    ok &= check(empty_fail.failing_index == 0U, "empty trajectory failure index should be zero");

    points[1].distance_m = std::numeric_limits<double>::infinity();
    auto non_finite_fail = validator.validate(points, count);
    ok &= check(!non_finite_fail.ok && non_finite_fail.violation == TrajectoryViolation::NullInput,
                "validator should reject non-finite trajectory fields");
    ok &= check(non_finite_fail.failing_index == 1U, "non-finite failure should point to offending index");
    points[1].distance_m = 0.0;

    points[2].speed_mps = 3.0;
    auto speed_fail     = validator.validate(points, count);
    ok &= check(!speed_fail.ok && speed_fail.violation == TrajectoryViolation::SpeedOutOfBounds,
                "validator should reject speed envelope violations");
    ok &= check(speed_fail.failing_index == 2U, "speed failure index should track violating point");

    points[2].speed_mps = 1.0;
    points[3].t_s       = points[2].t_s;
    auto time_fail      = validator.validate(points, count);
    ok &= check(!time_fail.ok && time_fail.violation == TrajectoryViolation::NonMonotonicTime,
                "validator should reject non-monotonic time");
    ok &= check(time_fail.failing_index == 3U, "time monotonic failure index should be precise");

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] motion trajectory tests" << '\n';
    return 0;
}
