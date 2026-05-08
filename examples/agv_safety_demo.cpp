// AGV Safety Demo — Full pipeline demonstration
//
// This example simulates a warehouse AGV navigating a corridor with obstacles.
// It demonstrates the complete safety-autonomy-core pipeline:
//   1. System configuration and context building
//   2. Sensor fusion (EKF + complementary filter)
//   3. Safety PID control with feedforward
//   4. Multi-zone safety envelope evaluation
//   5. Jerk-limited emergency stop generation
//   6. Safety supervisor monitoring and escalation
//   7. State machine transitions under fault conditions
//   8. Diagnostic event publishing

#include "safety_core/common/time.hpp"
#include "safety_core/config/system_config.hpp"
#include "safety_core/control/safety_pid.hpp"
#include "safety_core/diag/logging_diagnostic_transport.hpp"
#include "safety_core/diag/logging_health_monitor.hpp"
#include "safety_core/exec/task_executor.hpp"
#include "safety_core/filters/bounded_ekf_filter.hpp"
#include "safety_core/filters/complementary_filter.hpp"
#include "safety_core/motion/trajectory.hpp"
#include "safety_core/platform/actuators/drive_actuator.hpp"
#include "safety_core/platform/manual_clock.hpp"
#include "safety_core/platform/sensors/imu_sensor.hpp"
#include "safety_core/platform/sensors/odometry_sensor.hpp"
#include "safety_core/safety/safety_envelope.hpp"
#include "safety_core/safety/safety_supervisor.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "safety_core/system/context_factory.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>

using namespace safety_core;
using namespace std::chrono_literals;

// ============================================================
// AGV Simulation Parameters
// ============================================================

struct SimulationState
{
    double position_m{0.0};
    double velocity_mps{0.0};
    double heading_rad{0.0};
    double obstacle_distance_m{5.0}; // starts 5m ahead
    std::uint64_t time_ns{0U};
    std::uint32_t cycle{0U};
};

// ============================================================
// System Configuration for a Warehouse AGV
// ============================================================

static config::SystemConfig make_agv_config()
{
    config::SystemConfig cfg{};
    cfg.config_version                  = config::kCurrentSystemConfigVersion;
    cfg.timing.control_period           = 20ms;  // 50 Hz control loop
    cfg.timing.watchdog_period          = 40ms;  // 2x control period
    cfg.timing.localization_timeout     = 500ms; // 0.5s localization staleness
    cfg.envelope.max_speed_mps          = 1.5;   // 1.5 m/s max
    cfg.envelope.max_accel_mps2         = 0.75;  // 0.75 m/s^2
    cfg.envelope.max_comfort_decel_mps2 = 1.0;   // 1.0 m/s^2 comfort braking
    cfg.envelope.control_latency_s      = 0.04;  // 40ms latency budget
    cfg.envelope.safety_buffer_m        = 0.3;   // 30cm minimum clearance
    cfg.max_tasks                       = 4U;
    return cfg;
}

// ============================================================
// Simulated Sensor Data Generation
// ============================================================

static platform::sensors::ImuSample simulate_imu(const SimulationState& state, double commanded_accel)
{
    platform::sensors::ImuSample sample{};
    sample.linear_accel_x_mps2      = commanded_accel + 0.01; // small bias
    sample.linear_accel_y_mps2      = 0.002;                  // lateral noise
    sample.linear_accel_z_mps2      = 9.81;                   // gravity
    sample.angular_velocity_z_radps = 0.0;
    sample.timestamp_ns             = state.time_ns;
    sample.status                   = platform::sensors::imu_status::kValid;
    return sample;
}

static platform::sensors::OdometrySample simulate_odometry(const SimulationState& state)
{
    platform::sensors::OdometrySample sample{};
    sample.position_m   = state.position_m + 0.005 * std::sin(static_cast<double>(state.cycle)); // noise
    sample.velocity_mps = state.velocity_mps;
    sample.timestamp_ns = state.time_ns;
    return sample;
}

// ============================================================
// Print Helpers
// ============================================================

static const char* zone_name(safety::SafetyZone zone)
{
    switch (zone)
    {
    case safety::SafetyZone::Clear:
        return "CLEAR";
    case safety::SafetyZone::Warning:
        return "WARNING";
    case safety::SafetyZone::Protective:
        return "PROTECTIVE";
    case safety::SafetyZone::Emergency:
        return "EMERGENCY";
    default:
        return "UNKNOWN";
    }
}

static const char* mode_name(sm::Mode mode)
{
    switch (mode)
    {
    case sm::Mode::Init:
        return "Init";
    case sm::Mode::Idle:
        return "Idle";
    case sm::Mode::Moving:
        return "Moving";
    case sm::Mode::Degraded:
        return "Degraded";
    case sm::Mode::AvoidingObstacle:
        return "AvoidingObstacle";
    case sm::Mode::LocalizationLost:
        return "LocalizationLost";
    case sm::Mode::Docking:
        return "Docking";
    case sm::Mode::SafeStop:
        return "SafeStop";
    default:
        return "Unknown";
    }
}

// ============================================================
// Main Demo
// ============================================================

int main()
{
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║       AGV Safety Demo — safety-autonomy-core           ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════╣\n";
    std::cout << "║ Scenario: AGV navigating corridor, obstacle appears    ║\n";
    std::cout << "║ Demonstrates: fusion, PID, envelope, e-stop, faults    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";

    // --- 1. Build system context ---
    std::cout << "[1] Building system context...\n";
    diag::LoggingDiagnosticTransport transport(std::cout);
    diag::LoggingHealthMonitor health_monitor(std::cout);
    platform::ManualClock clock;
    clock.set(time::TimePoint{std::chrono::nanoseconds{0}});

    auto cfg = make_agv_config();

    sm::ModeStateMachine state_machine(&health_monitor, &transport);
    state_machine.set_clock(&clock);

    safety::SafetySupervisor supervisor(&state_machine, &transport, &clock);

    std::cout << "  Config: max_speed=" << cfg.envelope.max_speed_mps
              << " m/s, control_period=20ms, safety_buffer=" << cfg.envelope.safety_buffer_m << " m\n\n";

    // --- 2. Initialize components ---
    std::cout << "[2] Initializing components...\n";

    // EKF for position fusion with innovation gating
    filters::BoundedEkfFilter position_ekf;
    filters::BoundedEkfParams ekf_params{};
    ekf_params.process_noise_position = 1e-4;
    ekf_params.process_noise_velocity = 1e-3;
    ekf_params.measurement_noise      = 0.01;
    ekf_params.innovation_gate_sigma  = 3.0; // 3-sigma outlier rejection
    ekf_params.max_abs_position       = 100.0;
    ekf_params.max_abs_velocity       = 3.0;
    position_ekf.set_params(ekf_params);
    position_ekf.apply_config(cfg);
    position_ekf.reset(0.0, 0.0);

    // Complementary filter for heading (gyro rate + absolute)
    filters::ComplementaryFilter heading_filter;
    filters::ComplementaryFilterParams comp_params{};
    comp_params.cutoff_frequency_hz = 0.5; // 0.5 Hz cutoff
    comp_params.max_abs_state       = 6.3; // ±π
    heading_filter.set_params(comp_params);
    heading_filter.apply_config(cfg);
    heading_filter.reset(0.0);

    // Safety PID with anti-windup
    control::SafetyPidController pid(&clock);
    pid.apply_config(cfg);
    pid.set_gains({2.0, 0.5, 0.1, 3.0}); // Kp=2, Ki=0.5, Kd=0.1, max_integral=3
    pid.set_diagnostic_transport(&transport);
    pid.mark_localization_update(clock.now());

    // Drive actuator
    platform::actuators::BufferedDriveActuator drive;

    // Sensor buffers
    platform::sensors::BufferedImuSensor imu_sensor;
    platform::sensors::BufferedOdometrySensor odom_sensor;

    // Robot footprint (typical warehouse AGV)
    safety::RobotFootprint footprint{};
    footprint.length_m         = 1.0;
    footprint.width_m          = 0.7;
    footprint.front_overhang_m = 0.15;

    std::cout << "  EKF: gate_sigma=3.0, process_noise=[1e-4, 1e-3], meas_noise=0.01\n";
    std::cout << "  PID: Kp=2.0, Ki=0.5, Kd=0.1, max_integral=3.0\n";
    std::cout << "  Footprint: L=1.0m, W=0.7m, overhang=0.15m\n\n";

    // --- 3. Transition to Moving ---
    std::cout << "[3] Starting mission: Init → Idle → Moving\n";
    state_machine.transition_to(sm::Mode::Idle);
    state_machine.transition_to(sm::Mode::Moving);
    std::cout << "  Mode: " << mode_name(state_machine.mode()) << "\n\n";

    // --- 4. Run control loop ---
    std::cout << "[4] Running control loop (50 Hz, obstacle approaching)...\n";
    std::cout << "─────────────────────────────────────────────────────────────────────────\n";
    std::cout << "  Cycle | Pos(m) | Vel(m/s) | Obstacle(m) | Zone       | PID Out | Mode\n";
    std::cout << "─────────────────────────────────────────────────────────────────────────\n";

    SimulationState sim{};
    double target_speed           = 1.2; // m/s cruise speed
    double last_accel             = 0.0;
    bool obstacle_event_triggered = false;
    bool estop_generated          = false;

    for (std::uint32_t cycle = 0; cycle < 300; ++cycle)
    {
        sim.cycle   = cycle;
        sim.time_ns = static_cast<std::uint64_t>(cycle) * 20000000ULL; // 20ms per cycle
        clock.set(time::TimePoint{std::chrono::nanoseconds{sim.time_ns}});

        // Simulate obstacle getting closer (AGV moving toward it)
        sim.obstacle_distance_m = 5.0 - sim.position_m;

        // At cycle 100, inject a GPS outlier to test innovation gating
        double odom_measurement = sim.position_m + 0.003 * std::sin(static_cast<double>(cycle) * 0.5);
        if (cycle == 100)
        {
            odom_measurement += 50.0; // 50m GPS jump — should be gated
        }

        // --- Sensor fusion ---
        auto imu_sample  = simulate_imu(sim, last_accel);
        auto odom_sample = simulate_odometry(sim);
        imu_sensor.write(imu_sample);
        odom_sensor.write(odom_sample);

        position_ekf.update(odom_measurement, imu_sample.linear_accel_x_mps2, sim.time_ns);
        heading_filter.update(imu_sample.angular_velocity_z_radps, 0.0, sim.time_ns);

        // --- Safety envelope check ---
        auto envelope_eval =
            safety::evaluate_stop_distance(sim.obstacle_distance_m, sim.velocity_mps, cfg.envelope, footprint);

        // --- Control decision based on safety zone ---
        double speed_setpoint = target_speed;
        double ff_velocity    = 0.0;
        double ff_accel       = 0.0;

        switch (envelope_eval.zone)
        {
        case safety::SafetyZone::Clear:
            speed_setpoint = target_speed;
            ff_velocity    = target_speed;
            break;
        case safety::SafetyZone::Warning:
            speed_setpoint = std::min(target_speed, envelope_eval.recommended_speed_limit_mps);
            ff_velocity    = speed_setpoint;
            break;
        case safety::SafetyZone::Protective:
            speed_setpoint = 0.0;
            ff_velocity    = 0.0;
            ff_accel       = -cfg.envelope.max_comfort_decel_mps2;

            if (!obstacle_event_triggered)
            {
                state_machine.request_obstacle_hold();
                obstacle_event_triggered = true;
            }
            break;
        case safety::SafetyZone::Emergency:
            speed_setpoint = 0.0;
            ff_velocity    = 0.0;
            ff_accel       = -cfg.envelope.max_comfort_decel_mps2;

            if (!estop_generated)
            {
                // Generate jerk-limited emergency stop profile
                constexpr std::size_t kStopCapacity = 100U;
                motion::TrajectoryPoint stop_profile[kStopCapacity]{};
                std::size_t stop_count = 0U;
                motion::generate_jerk_limited_stop_profile(sim.velocity_mps, cfg.envelope.max_comfort_decel_mps2 * 1.5,
                                                           15.0, 0.01, stop_profile, kStopCapacity, stop_count);
                std::cout << "\n  [E-STOP] Jerk-limited profile: " << stop_count
                          << " points, stop_dist=" << stop_profile[stop_count - 1].distance_m << " m\n";
                estop_generated = true;

                // Escalate to supervisor
                safety::MonitorEvent event{};
                event.type     = safety::MonitorType::TrajectoryEnvelope;
                event.severity = safety::MonitorSeverity::Critical;
                event.code     = 2001U;
                event.detail   = "emergency zone breach";
                supervisor.process_monitor_event(event);
            }
            break;
        }

        // --- PID compute with feedforward ---
        pid.mark_localization_update(clock.now());
        double pid_output = pid.compute(speed_setpoint, sim.velocity_mps, ff_velocity, ff_accel);

        // --- Command actuator ---
        platform::actuators::DriveCommand cmd{};
        cmd.velocity_mps = pid_output;
        cmd.steering_rad = 0.0;
        cmd.timestamp_ns = sim.time_ns;
        drive.command(cmd);

        // --- Verify command freshness ---
        bool cmd_fresh = drive.is_command_fresh(sim.time_ns, 40000000ULL); // 40ms timeout
        (void)cmd_fresh;

        // --- Physics update (simple 1D) ---
        const double dt = 0.02; // 20ms
        last_accel      = (pid_output - sim.velocity_mps) / dt;
        last_accel      = std::clamp(last_accel, -cfg.envelope.max_comfort_decel_mps2, cfg.envelope.max_accel_mps2);
        sim.velocity_mps += last_accel * dt;
        sim.velocity_mps = std::max(0.0, sim.velocity_mps);
        sim.position_m += sim.velocity_mps * dt;

        // Print every 10 cycles
        if (cycle % 10 == 0)
        {
            std::printf("  %5u | %6.3f | %8.3f | %11.3f | %-10s | %7.3f | %s\n", cycle, sim.position_m,
                        sim.velocity_mps, sim.obstacle_distance_m, zone_name(envelope_eval.zone), pid_output,
                        mode_name(state_machine.mode()));
        }

        // Stop if we've reached SafeStop and velocity is zero
        if (state_machine.mode() == sm::Mode::SafeStop && sim.velocity_mps < 0.001)
        {
            std::printf("  %5u | %6.3f | %8.3f | %11.3f | %-10s | %7.3f | %s\n", cycle, sim.position_m,
                        sim.velocity_mps, sim.obstacle_distance_m, zone_name(envelope_eval.zone), pid_output,
                        mode_name(state_machine.mode()));
            break;
        }
    }

    // --- 5. Summary ---
    std::cout << "─────────────────────────────────────────────────────────────────────────\n\n";
    std::cout << "[5] Mission Summary:\n";
    std::cout << "  Final position:     " << sim.position_m << " m\n";
    std::cout << "  Final velocity:     " << sim.velocity_mps << " m/s\n";
    std::cout << "  Obstacle distance:  " << sim.obstacle_distance_m << " m\n";
    std::cout << "  Final mode:         " << mode_name(state_machine.mode()) << "\n";
    std::cout << "  Fault latched:      " << (state_machine.fault_latched() ? "YES" : "NO") << "\n";
    std::cout << "  EKF rejected:       " << position_ekf.rejected_count() << " measurements\n";
    std::cout << "  EKF healthy:        " << (position_ekf.healthy() ? "YES" : "NO") << "\n";
    std::cout << "  Drive commands:     " << drive.command_count() << "\n";

    std::cout << "\n[OK] Demo completed successfully.\n";
    return 0;
}
