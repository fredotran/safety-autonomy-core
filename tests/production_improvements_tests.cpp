#include "safety_core/control/safety_pid.hpp"
#include "safety_core/filters/alpha_beta_filter.hpp"
#include "safety_core/filters/bounded_ekf_filter.hpp"
#include "safety_core/filters/complementary_filter.hpp"
#include "safety_core/motion/trajectory.hpp"
#include "safety_core/platform/actuators/drive_actuator.hpp"
#include "safety_core/platform/manual_clock.hpp"
#include "safety_core/platform/sensors/imu_sensor.hpp"
#include "safety_core/platform/sensors/odometry_sensor.hpp"
#include "safety_core/safety/safety_envelope.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "test_support.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace safety_core;
using namespace safety_core::test_support;

// ============================================================
// PID Anti-Windup Tests
// ============================================================

static bool test_pid_integral_clamped_under_sustained_error()
{
    platform::ManualClock clock;
    clock.set(time::TimePoint{std::chrono::nanoseconds{0}});

    control::SafetyPidController pid(&clock);
    auto cfg = make_motion_defaults();
    pid.apply_config(cfg);
    pid.set_gains({1.0, 1.0, 0.0, 5.0}); // max_integral = 5.0
    pid.mark_localization_update(clock.now());

    // Sustain a large error for many cycles — integral should clamp
    for (int i = 0; i < 100; ++i)
    {
        clock.advance(std::chrono::milliseconds{100});
        pid.mark_localization_update(clock.now());
        pid.compute(100.0, 0.0); // large error
    }

    // Output should be bounded by speed limit, not blow up
    clock.advance(std::chrono::milliseconds{100});
    pid.mark_localization_update(clock.now());
    double output = pid.compute(100.0, 0.0);
    if (!check(std::abs(output) <= cfg.envelope.max_speed_mps + 0.01, "PID output bounded under sustained error"))
    {
        return false;
    }
    return true;
}

static bool test_pid_feedforward_adds_velocity()
{
    platform::ManualClock clock;
    clock.set(time::TimePoint{std::chrono::nanoseconds{0}});

    control::SafetyPidController pid(&clock);
    auto cfg = make_motion_defaults();
    pid.apply_config(cfg);
    pid.set_gains({0.0, 0.0, 0.0, 100.0}); // zero PID gains
    pid.mark_localization_update(clock.now());

    clock.advance(std::chrono::milliseconds{100});
    pid.mark_localization_update(clock.now());

    // With zero gains, output should be purely from feedforward
    double output = pid.compute(0.0, 0.0, 1.0, 0.0);
    if (!check(std::abs(output - 1.0) < 0.01, "feedforward velocity passes through"))
    {
        return false;
    }
    return true;
}

// ============================================================
// Drive Command Freshness Tests
// ============================================================

static bool test_drive_command_freshness()
{
    platform::actuators::BufferedDriveActuator actuator;

    // No command yet — should not be fresh
    if (!check(!actuator.is_command_fresh(1000000U, 50000000U), "no command is not fresh"))
    {
        return false;
    }

    // Send a command at t=100ms
    actuator.command({1.0, 0.0, 100000000U});
    if (!check(actuator.is_command_fresh(120000000U, 50000000U), "recent command is fresh"))
    {
        return false;
    }

    // Check stale at t=200ms (100ms old, threshold 50ms)
    if (!check(!actuator.is_command_fresh(200000000U, 50000000U), "old command is stale"))
    {
        return false;
    }

    if (!check(actuator.command_count() == 1U, "command count tracks"))
    {
        return false;
    }
    return true;
}

// ============================================================
// State Machine Atomic Thread Safety Tests
// ============================================================

static bool test_state_machine_atomic_reads()
{
    sm::ModeStateMachine machine;
    if (!check(machine.mode() == sm::Mode::Init, "initial mode is Init"))
    {
        return false;
    }
    if (!check(!machine.fault_latched(), "initial fault_latched is false"))
    {
        return false;
    }

    machine.transition_to(sm::Mode::Idle);
    if (!check(machine.mode() == sm::Mode::Idle, "mode transitions correctly"))
    {
        return false;
    }

    machine.latch_fault(999U);
    if (!check(machine.fault_latched(), "fault_latched after latch"))
    {
        return false;
    }
    if (!check(machine.mode() == sm::Mode::SafeStop, "mode is SafeStop after fault"))
    {
        return false;
    }
    return true;
}

// ============================================================
// EKF Innovation Gating Tests
// ============================================================

static bool test_ekf_innovation_gating_rejects_outlier()
{
    filters::BoundedEkfFilter ekf;
    filters::BoundedEkfParams params{};
    params.measurement_noise     = 1.0;
    params.innovation_gate_sigma = 3.0; // 3-sigma gate
    ekf.set_params(params);
    ekf.reset(0.0, 0.0);

    // Normal update should pass
    ekf.update(0.5);
    if (!check(ekf.rejected_count() == 0U, "normal measurement not rejected"))
    {
        return false;
    }

    // Massive outlier (100m jump) should be gated
    ekf.update(100.0);
    if (!check(ekf.rejected_count() == 1U, "outlier measurement rejected"))
    {
        return false;
    }

    // Position should not have jumped to 100
    if (!check(std::abs(ekf.position()) < 5.0, "position stable after outlier rejection"))
    {
        return false;
    }
    return true;
}

static bool test_ekf_timestamped_update()
{
    filters::BoundedEkfFilter ekf;
    auto cfg = make_motion_defaults();
    ekf.apply_config(cfg);
    ekf.reset(0.0, 1.0); // initial velocity 1 m/s

    // Feed measurements at 50ms intervals
    std::uint64_t t = 1000000000ULL; // 1 second
    for (int i = 0; i < 10; ++i)
    {
        t += 50000000ULL; // 50ms
        double expected_pos = 1.0 * (static_cast<double>(i + 1) * 0.05);
        ekf.update(expected_pos, 0.0, t);
    }

    if (!check(ekf.healthy(), "EKF healthy after timestamped updates"))
    {
        return false;
    }
    if (!check(std::abs(ekf.position() - 0.5) < 0.2, "EKF tracks position with timestamps"))
    {
        return false;
    }
    return true;
}

// ============================================================
// Complementary Filter Cutoff Frequency Tests
// ============================================================

static bool test_complementary_filter_cutoff_frequency()
{
    filters::ComplementaryFilter filter;
    filters::ComplementaryFilterParams params{};
    params.cutoff_frequency_hz = 1.0; // 1 Hz cutoff
    params.max_abs_state       = 1000.0;
    filter.set_params(params);
    filter.reset(0.0);

    auto cfg = make_motion_defaults();
    filter.apply_config(cfg);

    // With 1Hz cutoff and 100ms dt: tau = 1/(2*pi) ≈ 0.159, alpha = 0.159/(0.159+0.1) ≈ 0.614
    // Feed constant rate=0, absolute=10 — should converge to 10
    for (int i = 0; i < 100; ++i)
    {
        filter.update(0.0, 10.0);
    }

    if (!check(std::abs(filter.state() - 10.0) < 0.1, "complementary filter converges with cutoff freq"))
    {
        return false;
    }
    return true;
}

static bool test_complementary_filter_timestamped()
{
    filters::ComplementaryFilter filter;
    filters::ComplementaryFilterParams params{};
    params.alpha = 0.9;
    filter.set_params(params);
    filter.reset(0.0);

    std::uint64_t t = 1000000000ULL;
    for (int i = 0; i < 20; ++i)
    {
        t += 50000000ULL; // 50ms intervals
        filter.update(0.0, 5.0, t);
    }

    if (!check(std::abs(filter.state() - 5.0) < 1.0, "timestamped complementary converges"))
    {
        return false;
    }
    return true;
}

// ============================================================
// Alpha-Beta Filter Timestamped Tests
// ============================================================

static bool test_alpha_beta_timestamped()
{
    filters::AlphaBetaFilter filter;
    filter.set_params({0.8, 0.1});
    filter.reset(0.0, 1.0); // velocity 1 m/s

    std::uint64_t t = 0ULL;
    for (int i = 0; i < 20; ++i)
    {
        t += 100000000ULL;                                    // 100ms
        double meas = 1.0 * static_cast<double>(i + 1) * 0.1; // linear ramp
        filter.update(meas, t);
    }

    if (!check(std::abs(filter.velocity() - 1.0) < 0.3, "alpha-beta tracks velocity with timestamps"))
    {
        return false;
    }
    return true;
}

// ============================================================
// IMU Sensor 6-DOF + Seqlock Tests
// ============================================================

static bool test_imu_6dof_seqlock()
{
    platform::sensors::BufferedImuSensor imu;

    platform::sensors::ImuSample sample{};
    sample.angular_velocity_x_radps = 0.01;
    sample.angular_velocity_y_radps = 0.02;
    sample.angular_velocity_z_radps = 0.5;
    sample.linear_accel_x_mps2      = 0.1;
    sample.linear_accel_y_mps2      = -0.05;
    sample.linear_accel_z_mps2      = 9.81;
    sample.timestamp_ns             = 1000000000ULL;
    sample.status                   = platform::sensors::imu_status::kValid;

    imu.write(sample);
    auto read_sample = imu.read();

    if (!check(std::abs(read_sample.linear_accel_z_mps2 - 9.81) < 0.001, "6-DOF z-accel correct"))
    {
        return false;
    }
    if (!check(read_sample.status == platform::sensors::imu_status::kValid, "status flag correct"))
    {
        return false;
    }
    if (!check(imu.is_fresh(1050000000ULL, 100000000ULL), "IMU is fresh within window"))
    {
        return false;
    }
    if (!check(!imu.is_fresh(1200000000ULL, 100000000ULL), "IMU is stale outside window"))
    {
        return false;
    }
    return true;
}

// ============================================================
// Odometry Increment Tests
// ============================================================

static bool test_odometry_increment()
{
    platform::sensors::BufferedOdometrySensor odom;

    platform::sensors::OdometryIncrement inc{};
    inc.delta_x_m              = 0.01;
    inc.delta_y_m              = 0.005;
    inc.delta_yaw_rad          = 0.001;
    inc.linear_velocity_mps    = 0.5;
    inc.angular_velocity_radps = 0.1;
    inc.timestamp_ns           = 500000000ULL;

    odom.write_increment(inc);
    auto read_inc = odom.read_increment();

    if (!check(std::abs(read_inc.delta_x_m - 0.01) < 1e-9, "increment delta_x correct"))
    {
        return false;
    }
    if (!check(std::abs(read_inc.angular_velocity_radps - 0.1) < 1e-9, "increment angular vel correct"))
    {
        return false;
    }
    return true;
}

// ============================================================
// Multi-Zone Safety Envelope Tests
// ============================================================

static bool test_multi_zone_safety_envelope()
{
    config::MotionEnvelopeConfig envelope{};
    envelope.max_speed_mps          = 1.5;
    envelope.max_accel_mps2         = 0.75;
    envelope.max_comfort_decel_mps2 = 1.0;
    envelope.control_latency_s      = 0.1;
    envelope.safety_buffer_m        = 0.2;

    // Far away: Clear zone
    auto eval = safety::evaluate_stop_distance(10.0, 1.0, envelope);
    if (!check(eval.zone == safety::SafetyZone::Clear, "far obstacle is Clear zone"))
    {
        return false;
    }
    if (!check(eval.within_envelope, "far obstacle within envelope"))
    {
        return false;
    }

    // Close but stoppable: Warning zone
    // Required = 1.0*0.1 + 1.0^2/(2*1.0) + 0.2 = 0.1 + 0.5 + 0.2 = 0.8
    // Warning threshold = 0.8 * 1.5 = 1.2
    eval = safety::evaluate_stop_distance(1.0, 1.0, envelope);
    if (!check(eval.zone == safety::SafetyZone::Warning, "close obstacle is Warning zone"))
    {
        return false;
    }
    if (!check(eval.recommended_speed_limit_mps > 0.0, "warning zone has speed limit"))
    {
        return false;
    }

    // Too close: Protective zone
    eval = safety::evaluate_stop_distance(0.5, 1.0, envelope);
    if (!check(eval.zone == safety::SafetyZone::Protective, "too close is Protective zone"))
    {
        return false;
    }

    // Inside buffer: Emergency zone
    eval = safety::evaluate_stop_distance(0.1, 1.0, envelope);
    if (!check(eval.zone == safety::SafetyZone::Emergency, "inside buffer is Emergency zone"))
    {
        return false;
    }
    return true;
}

static bool test_robot_footprint_envelope()
{
    config::MotionEnvelopeConfig envelope{};
    envelope.max_comfort_decel_mps2 = 1.0;
    envelope.control_latency_s      = 0.1;
    envelope.safety_buffer_m        = 0.2;

    safety::RobotFootprint footprint{};
    footprint.front_overhang_m = 0.3;

    // With footprint overhang, effective distance is reduced
    auto eval_no_fp = safety::evaluate_stop_distance(1.0, 1.0, envelope);
    auto eval_fp    = safety::evaluate_stop_distance(1.0, 1.0, envelope, footprint);

    if (!check(eval_fp.required_clearance == eval_no_fp.required_clearance,
               "clearance calculation same regardless of footprint"))
    {
        return false;
    }
    // The footprint version uses reduced effective distance, so is more conservative
    if (!check(!eval_fp.within_envelope || eval_no_fp.within_envelope, "footprint version is more conservative"))
    {
        return false;
    }
    return true;
}

// ============================================================
// Jerk-Limited Emergency Stop Tests
// ============================================================

static bool test_jerk_limited_stop_profile()
{
    constexpr std::size_t kCapacity = 200U;
    motion::TrajectoryPoint points[kCapacity]{};
    std::size_t count = 0U;

    bool ok = motion::generate_jerk_limited_stop_profile(1.5,  // initial speed m/s
                                                         2.0,  // max decel m/s^2
                                                         10.0, // max jerk m/s^3
                                                         0.01, // dt = 10ms
                                                         points, kCapacity, count);

    if (!check(ok, "jerk-limited stop profile generated"))
    {
        return false;
    }
    if (!check(count > 2U, "profile has multiple points"))
    {
        return false;
    }

    // Last point should be stopped
    if (!check(std::abs(points[count - 1].speed_mps) < 0.001, "final speed is zero"))
    {
        return false;
    }

    // First point should not have max decel immediately (jerk limiting)
    if (!check(std::abs(points[0].accel_mps2) < 2.0, "initial accel is not at max (jerk limited)"))
    {
        return false;
    }

    // Verify monotonically decreasing speed
    for (std::size_t i = 1; i < count; ++i)
    {
        if (!check(points[i].speed_mps <= points[i - 1].speed_mps + 0.001, "speed monotonically decreasing"))
        {
            return false;
        }
    }
    return true;
}

static bool test_jerk_limited_vs_standard_stop_distance()
{
    constexpr std::size_t kCapacity = 200U;
    motion::TrajectoryPoint jerk_points[kCapacity]{};
    motion::TrajectoryPoint std_points[kCapacity]{};
    std::size_t jerk_count = 0U;
    std::size_t std_count  = 0U;

    motion::generate_jerk_limited_stop_profile(1.0, 1.5, 8.0, 0.01, jerk_points, kCapacity, jerk_count);
    motion::generate_emergency_stop_profile(1.0, 1.5, 0.01, std_points, kCapacity, std_count);

    // Jerk-limited should take longer distance (less aggressive start)
    double jerk_dist = jerk_points[jerk_count - 1].distance_m;
    double std_dist  = std_points[std_count - 1].distance_m;

    if (!check(jerk_dist >= std_dist, "jerk-limited stop takes >= distance than standard"))
    {
        return false;
    }
    return true;
}

// ============================================================
// Main
// ============================================================

int main()
{
    int failures = 0;

    std::cout << "=== Production Improvements Tests ===\n";

    auto run = [&](bool (*fn)(), const char* name)
    {
        std::cout << "  " << name << "... ";
        if (fn())
        {
            std::cout << "PASS\n";
        }
        else
        {
            std::cout << "FAIL\n";
            ++failures;
        }
    };

    run(test_pid_integral_clamped_under_sustained_error, "PID anti-windup clamp");
    run(test_pid_feedforward_adds_velocity, "PID feedforward");
    run(test_drive_command_freshness, "Drive command freshness");
    run(test_state_machine_atomic_reads, "State machine atomics");
    run(test_ekf_innovation_gating_rejects_outlier, "EKF innovation gating");
    run(test_ekf_timestamped_update, "EKF timestamped update");
    run(test_complementary_filter_cutoff_frequency, "Complementary cutoff freq");
    run(test_complementary_filter_timestamped, "Complementary timestamped");
    run(test_alpha_beta_timestamped, "Alpha-beta timestamped");
    run(test_imu_6dof_seqlock, "IMU 6-DOF + seqlock");
    run(test_odometry_increment, "Odometry increment");
    run(test_multi_zone_safety_envelope, "Multi-zone safety envelope");
    run(test_robot_footprint_envelope, "Robot footprint envelope");
    run(test_jerk_limited_stop_profile, "Jerk-limited stop profile");
    run(test_jerk_limited_vs_standard_stop_distance, "Jerk-limited vs standard distance");

    std::cout << "\n";
    if (failures > 0)
    {
        std::cout << failures << " test(s) FAILED\n";
        return 1;
    }
    std::cout << "All production improvement tests PASSED\n";
    return 0;
}
