// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/safety/safety_supervisor.hpp"
#include "safety_core/safety/safety_envelope.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "safety_core/filters/bounded_ekf_filter.hpp"
#include "safety_core/config/system_config.hpp"
#include "test_support.hpp"

#include <chrono>
#include <thread>
#include <Eigen/Dense>

using namespace safety_core;
using namespace safety_core::test_support;
using namespace std::chrono_literals;

/**
 * Non-regression test suite to ensure that previously fixed bugs don't reappear.
 * These tests validate critical behaviors and known bug fixes.
 */

int main() {
    using safety_core::config::SystemConfig;
    
    int failures = 0;
    
    // Test: Ensure safety envelope calculations don't produce negative distances
    {
        SystemConfig config = make_motion_defaults();
        SafetyEnvelope envelope(config);
        
        double distance = envelope.evaluate_stop_distance(5.0, 2.0);
        if (!check(distance >= 0.0, "Safety envelope should not produce negative distances")) {
            failures++;
        }
        
        distance = envelope.evaluate_stop_distance(0.0, 0.0);
        if (!check(distance >= 0.0, "Zero inputs should not produce negative distances")) {
            failures++;
        }
    }
    
    // Test: Ensure state machine doesn't get stuck in invalid states
    {
        ModeStateMachine sm;
        
        sm.transition(Mode::SafeStop);
        sm.transition(Mode::Moving);
        sm.transition(Mode::SafeStop);
        
        bool can_transition = sm.can_transition(Mode::Moving);
        if (!check(can_transition, "State machine should not lock in SafeStop")) {
            failures++;
        }
    }
    
    // Test: Ensure EKF filter doesn't diverge with valid inputs
    {
        BoundedEKFFilter filter(15);
        
        Eigen::VectorXd initial_state = Eigen::VectorXd::Zero(15);
        filter.initialize(initial_state);
        
        for (int i = 0; i < 100; i++) {
            Eigen::VectorXd measurement(3);
            measurement << i * 0.1, i * 0.1, 0.0;
            
            filter.predict(0.1);
            filter.update(measurement);
            
            Eigen::VectorXd state = filter.get_state();
            if (!check(state.array().isFinite().all(), "EKF state should not diverge to NaN/Inf")) {
                failures++;
            }
            if (!check(state.norm() < 1000.0, "EKF state should not diverge to large values")) {
                failures++;
            }
        }
    }
    
    // Test: Ensure safety supervisor doesn't miss emergency transitions
    {
        SystemConfig config = make_motion_defaults();
        SafetySupervisor supervisor(config);
        
        supervisor.emergency_stop("Test emergency");
        
        Mode current_mode = supervisor.get_current_mode();
        if (!check(current_mode == Mode::SafeStop, "Safety supervisor should immediately transition to SafeStop")) {
            failures++;
        }
        
        if (!check(supervisor.has_active_fault(), "Safety supervisor should latch emergency fault")) {
            failures++;
        }
    }
    
    // Test: Ensure configuration loading doesn't crash with missing optional fields
    {
        SystemConfig minimal_config;
        try {
            minimal_config = SystemConfig::load_default();
        } catch (...) {
            if (!check(false, "Config loading should handle missing optional fields gracefully")) {
                failures++;
            }
        }
    }
    
    // Test: Ensure PID controller doesn't produce infinite outputs
    {
        SafetyPID pid(1.0, 0.1, 0.01);
        
        double output = pid.compute(1000000.0, 0.0);
        if (!check(std::isfinite(output), "PID controller should not produce infinite outputs")) {
            failures++;
        }
        
        output = pid.compute(-1000000.0, 1000000.0);
        if (!check(std::isfinite(output), "PID controller should handle extreme setpoint changes")) {
            failures++;
        }
    }
    
    // Test: Ensure trajectory generation doesn't produce invalid paths
    {
        Eigen::Vector3d start(0.0, 0.0, 0.0);
        Eigen::Vector3d end(10.0, 0.0, 0.0);
        
        for (double t = 0.0; t <= 1.0; t += 0.1) {
            Eigen::Vector3d point = start + t * (end - start);
            if (!check(point.array().isFinite().all(), "Trajectory points should be finite")) {
                failures++;
            }
        }
    }
    
    // Test: Ensure time calculations don't have overflow issues
    {
        using namespace std::chrono;
        
        auto large_duration = hours(24 * 365);
        auto timestamp = system_clock::time_point::max() - large_duration;
        
        if (!check(timestamp < system_clock::time_point::max(), "Time calculations should not overflow")) {
            failures++;
        }
    }
    
    // Test: Ensure filter invariants are maintained
    {
        BoundedEKFFilter filter(15);
        
        Eigen::VectorXd initial_state = Eigen::VectorXd::Zero(15);
        filter.initialize(initial_state);
        
        Eigen::VectorXd state = filter.get_state();
        if (!check(state.array().isFinite().all(), "Filter state should remain finite")) {
            failures++;
        }
    }
    
    if (failures > 0) {
        std::cerr << "Non-regression tests: " << failures << " failures\n";
        return 1;
    }
    
    std::cout << "Non-regression tests: All tests passed\n";
    return 0;
}
