// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include "safety_core/safety/safety_envelope.hpp"
#include "safety_core/safety/safety_supervisor.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "safety_core_ros/param_loader.hpp"
#include "safety_core_ros/qos_config.hpp"
#include "safety_core_ros/ros_clock.hpp"
#include "safety_core_ros/ros_diagnostic_transport.hpp"
#include "safety_core_ros/ros_health_monitor.hpp"
#include "safety_core_ros/time_utils.hpp"

#include <memory>
#include <nav_msgs/msg/odometry.hpp>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <safety_core_msgs/msg/diagnostic_event.hpp>
#include <safety_core_msgs/msg/envelope_status.hpp>
#include <safety_core_msgs/msg/safety_state.hpp>
#include <safety_core_msgs/msg/sensor_health.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_srvs/srv/trigger.hpp>

namespace safety_core_ros
{

    /**
     * @brief Safety supervisor node for state machine management
     *
     * Owns the safety_core ModeStateMachine + SafetySupervisor and exposes them
     * through ROS topics. Triggers state transitions in response to envelope
     * zone changes and localization staleness, latches faults, and emits a
     * /safety/safe_stop boolean that downstream actuator bridges respect.
     *
     * The supervisor coordinates the overall safety system by:
     * - Monitoring envelope status and triggering appropriate state transitions
     * - Checking localization staleness and triggering LocalizationLost mode
     * - Latching faults on critical safety violations
     * - Publishing safety state and safe stop signals
     */
    class SafetySupervisorNode : public rclcpp::Node
    {
      public:
        explicit SafetySupervisorNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions{});

      private:
        // Callbacks
        void on_envelope(const safety_core_msgs::msg::EnvelopeStatus::ConstSharedPtr msg);
        void on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg);
        void on_sensor_health(const safety_core_msgs::msg::SensorHealth::ConstSharedPtr msg);
        void on_clear_fault(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
                            std::shared_ptr<std_srvs::srv::Trigger::Response> response);
        void timer_tick();

        // State management
        void handle_zone_transition(safety_core::safety::SafetyZone new_zone);
        void check_localization_staleness();
        void check_sensor_health();
        void publish_state(safety_core::sm::Mode current_mode);

        // State message building
        safety_core_msgs::msg::SafetyState build_state_msg(safety_core::sm::Mode current_mode) const;

        // Time utilities
        [[nodiscard]] std::uint64_t now_ns() const noexcept;

        // Supervisor + state machine ownership
        std::unique_ptr<RosClock> clock_;
        std::unique_ptr<RosHealthMonitor> health_monitor_;
        std::shared_ptr<RosDiagnosticTransport> diagnostic_transport_;
        std::unique_ptr<safety_core::sm::ModeStateMachine> machine_;
        std::unique_ptr<safety_core::safety::SafetySupervisor> supervisor_;

        // Configuration
        struct Params
        {
            double localization_timeout_s{0.5};
            bool auto_recover_from_obstacle{true};
            double sensor_timeout_s{1.0};
            double degraded_recovery_s{5.0};
        } params_;

        // State
        safety_core::safety::SafetyZone latest_zone_{safety_core::safety::SafetyZone::Clear};
        std::optional<std::uint64_t> last_odom_time_ns_;
        std::uint64_t localization_timeout_ns_{500'000'000ULL};
        bool safe_stop_requested_{false};

        // Sensor health tracking
        uint8_t worst_sensor_status_{safety_core_msgs::msg::SensorHealth::UNKNOWN};
        std::optional<std::uint64_t> last_sensor_health_time_ns_;
        std::uint64_t sensor_timeout_ns_{1'000'000'000ULL};
        std::uint64_t degraded_recovery_ns_{5'000'000'000ULL};
        bool in_degraded_mode_{false};

        // ROS interfaces
        rclcpp::Subscription<safety_core_msgs::msg::EnvelopeStatus>::SharedPtr envelope_sub_;
        rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
        rclcpp::Subscription<safety_core_msgs::msg::SensorHealth>::SharedPtr sensor_health_sub_;
        rclcpp::Publisher<safety_core_msgs::msg::SafetyState>::SharedPtr state_pub_;
        rclcpp::Publisher<safety_core_msgs::msg::DiagnosticEvent>::SharedPtr diag_pub_;
        rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr safe_stop_pub_;
        rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr clear_fault_srv_;
        rclcpp::TimerBase::SharedPtr timer_;
    };

} // namespace safety_core_ros
