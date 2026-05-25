// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/safety_supervisor_node.hpp"

#include "safety_core/config/validation.hpp"

#include <chrono>
#include <stdexcept>

using namespace std::chrono_literals;
using safety_core::safety::SafetyZone;
using safety_core::sm::Mode;
using std::placeholders::_1;
using std::placeholders::_2;

namespace safety_core_ros
{

    SafetySupervisorNode::SafetySupervisorNode(const rclcpp::NodeOptions& options)
        : rclcpp::Node("safety_supervisor_node", options)
    {
        // Load parameters
        params_.localization_timeout_s     = ParamLoader::load_double(this, "localization_timeout_s", 0.5);
        params_.auto_recover_from_obstacle = ParamLoader::load_bool(this, "auto_recover_from_obstacle", true);
        params_.sensor_timeout_s           = ParamLoader::load_double(this, "sensor_timeout_s", 1.0);
        params_.degraded_recovery_s        = ParamLoader::load_double(this, "degraded_recovery_s", 5.0);
        localization_timeout_ns_           = TimeUtils::seconds_to_nanoseconds(params_.localization_timeout_s);
        sensor_timeout_ns_                 = TimeUtils::seconds_to_nanoseconds(params_.sensor_timeout_s);
        degraded_recovery_ns_              = TimeUtils::seconds_to_nanoseconds(params_.degraded_recovery_s);

        // Validate localization_timeout_s parameter
        if (params_.localization_timeout_s <= 0.0)
        {
            RCLCPP_ERROR(get_logger(), "Invalid localization_timeout_s: %.3f (must be positive)",
                         params_.localization_timeout_s);
            throw std::runtime_error("localization_timeout_s must be positive");
        }

        if (params_.localization_timeout_s >= 10.0)
        {
            RCLCPP_WARN(get_logger(), "Unusually high localization_timeout_s: %.3fs (recommended < 10s)",
                        params_.localization_timeout_s);
        }

        if (params_.sensor_timeout_s <= 0.0)
        {
            RCLCPP_ERROR(get_logger(), "Invalid sensor_timeout_s: %.3f (must be positive)", params_.sensor_timeout_s);
            throw std::runtime_error("sensor_timeout_s must be positive");
        }

        // Create ROS interfaces with standardized QoS
        diag_pub_ =
            create_publisher<safety_core_msgs::msg::DiagnosticEvent>("safety/diagnostics", QosConfig::state_qos());
        state_pub_     = create_publisher<safety_core_msgs::msg::SafetyState>("safety/state", QosConfig::state_qos());
        safe_stop_pub_ = create_publisher<std_msgs::msg::Bool>("safety/safe_stop", QosConfig::state_qos());

        envelope_sub_ = create_subscription<safety_core_msgs::msg::EnvelopeStatus>(
            "safety/envelope_status", QosConfig::sensor_qos(), std::bind(&SafetySupervisorNode::on_envelope, this, _1));
        odom_sub_          = create_subscription<nav_msgs::msg::Odometry>("odom", QosConfig::sensor_qos(),
                                                                          std::bind(&SafetySupervisorNode::on_odom, this, _1));
        sensor_health_sub_ = create_subscription<safety_core_msgs::msg::SensorHealth>(
            "safety/sensor_health", QosConfig::sensor_qos(),
            std::bind(&SafetySupervisorNode::on_sensor_health, this, _1));

        // Create clear fault service for simulation/testing
        clear_fault_srv_ = create_service<std_srvs::srv::Trigger>(
            "safety/clear_fault", std::bind(&SafetySupervisorNode::on_clear_fault, this, _1, _2));

        // Create timer for periodic checks
        timer_ = create_wall_timer(50ms, std::bind(&SafetySupervisorNode::timer_tick, this));

        // Initialize safety core components
        clock_                = std::make_unique<RosClock>(get_clock());
        health_monitor_       = std::make_unique<RosHealthMonitor>(get_logger());
        diagnostic_transport_ = std::make_shared<RosDiagnosticTransport>(diag_pub_);

        machine_ =
            std::make_unique<safety_core::sm::ModeStateMachine>(health_monitor_.get(), diagnostic_transport_.get());
        machine_->set_clock(clock_.get());

        supervisor_ = std::make_unique<safety_core::safety::SafetySupervisor>(
            machine_.get(), diagnostic_transport_.get(), clock_.get());

        // Clear any latched fault from previous runs (for simulation/testing)
        if (machine_->fault_latched())
        {
            const safety_core::Result clear_result = machine_->clear_fault();
            if (clear_result.ok())
            {
                RCLCPP_INFO(get_logger(), "Cleared latched fault from previous run");
            }
            else
            {
                RCLCPP_WARN(get_logger(), "Failed to clear fault: %s", clear_result.message.data());
            }
        }

        // Initialize in Idle state
        (void)machine_->transition_to(Mode::Idle);

        RCLCPP_INFO(get_logger(),
                    "safety_supervisor_node initialized | localization_timeout=%.3fs sensor_timeout=%.3fs "
                    "degraded_recovery=%.3fs",
                    params_.localization_timeout_s, params_.sensor_timeout_s, params_.degraded_recovery_s);
    }

    std::uint64_t SafetySupervisorNode::now_ns() const noexcept
    {
        return TimeUtils::now_nanoseconds(get_clock());
    }

    void SafetySupervisorNode::on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg)
    {
        const std::uint64_t stamp_ns = static_cast<std::uint64_t>(msg->header.stamp.sec) * 1'000'000'000ULL +
                                       static_cast<std::uint64_t>(msg->header.stamp.nanosec);
        last_odom_time_ns_ = stamp_ns;
    }

    void SafetySupervisorNode::on_envelope(const safety_core_msgs::msg::EnvelopeStatus::ConstSharedPtr msg)
    {
        latest_zone_            = static_cast<SafetyZone>(msg->zone.zone);
        const Mode current_mode = machine_->mode();

        // Always check for auto-transition from Idle to Moving when in Clear zone
        // This ensures the transition happens even if the zone doesn't change
        if (current_mode == Mode::Idle && latest_zone_ == SafetyZone::Clear)
        {
            (void)machine_->transition_to(Mode::Moving);
            safe_stop_requested_ = false;
            RCLCPP_INFO(get_logger(), "Auto-transition from Idle to Moving (Clear zone)");
        }

        handle_zone_transition(latest_zone_);
    }

    void SafetySupervisorNode::handle_zone_transition(safety_core::safety::SafetyZone new_zone)
    {
        const Mode current_mode = machine_->mode();

        switch (new_zone)
        {
        case SafetyZone::Clear:
        case SafetyZone::Warning:
            // Recover from obstacle hold if path is clear enough
            if (current_mode == Mode::AvoidingObstacle && params_.auto_recover_from_obstacle)
            {
                (void)machine_->transition_to(Mode::Moving);
                safe_stop_requested_ = false;
            }
            break;

        case SafetyZone::Protective:
            // Request obstacle hold when in Protective zone
            if (current_mode == Mode::Moving)
            {
                (void)machine_->request_obstacle_hold();
            }
            break;

        case SafetyZone::Emergency:
            // Latch fault and transition to SafeStop
            // Fault code 0xE001 == "envelope emergency zone breached"
            (void)machine_->latch_fault(0xE001U);
            (void)machine_->transition_to(Mode::SafeStop);
            safe_stop_requested_ = true;
            break;
        }
    }

    void SafetySupervisorNode::timer_tick()
    {
        // Cache current time and mode to avoid repeated calls
        const std::uint64_t current_time_ns = now_ns();
        const Mode current_mode             = machine_->mode();

        // Check localization staleness
        check_localization_staleness();

        // Check sensor health
        check_sensor_health();

        // Observe clock sample
        (void)supervisor_->observe_clock_sample(current_time_ns);

        // Publish current state
        publish_state(current_mode);
    }

    void SafetySupervisorNode::on_sensor_health(const safety_core_msgs::msg::SensorHealth::ConstSharedPtr msg)
    {
        // Only track the "all" summary message for overall system health
        if (msg->sensor_name == "all")
        {
            worst_sensor_status_        = msg->status;
            last_sensor_health_time_ns_ = now_ns();
        }
    }

    void SafetySupervisorNode::check_sensor_health()
    {
        const Mode current_mode = machine_->mode();
        if (current_mode == Mode::SafeStop)
        {
            return; // Do not transition out of SafeStop based on sensor health
        }

        // Check if sensor health data is stale
        if (last_sensor_health_time_ns_.has_value())
        {
            const std::uint64_t current_time_ns = now_ns();
            if (TimeUtils::is_stale(last_sensor_health_time_ns_.value(), current_time_ns, sensor_timeout_ns_))
            {
                // Sensor monitor not running or not publishing - ignore
                return;
            }
        }
        else
        {
            // No sensor health received yet - ignore until we have data
            return;
        }

        // React to sensor health status
        if (worst_sensor_status_ == safety_core_msgs::msg::SensorHealth::FAULT)
        {
            // Critical sensor fault - transition to SafeStop
            if (current_mode != Mode::SafeStop)
            {
                RCLCPP_ERROR(get_logger(), "Sensor fault detected - transitioning to SafeStop");
                (void)machine_->latch_fault(0xE002U); // Fault code for sensor fault
                (void)machine_->transition_to(Mode::SafeStop);
                safe_stop_requested_ = true;
                in_degraded_mode_    = false;
            }
        }
        else if (worst_sensor_status_ == safety_core_msgs::msg::SensorHealth::DEGRADED)
        {
            if (current_mode == Mode::Moving && !in_degraded_mode_)
            {
                RCLCPP_WARN(get_logger(), "Sensor health degraded - transitioning to Degraded mode");
                (void)machine_->transition_to(Mode::Degraded);
                in_degraded_mode_ = true;
            }
        }
        else if (worst_sensor_status_ == safety_core_msgs::msg::SensorHealth::HEALTHY && in_degraded_mode_)
        {
            // Check if we've been in degraded mode long enough to recover
            // For simplicity, recover immediately when health returns
            // (The degraded_recovery_s parameter can be used for a timer-based approach in future)
            if (current_mode == Mode::Degraded)
            {
                RCLCPP_INFO(get_logger(), "Sensor health recovered - transitioning back to Moving");
                (void)machine_->transition_to(Mode::Moving);
                in_degraded_mode_ = false;
            }
        }
    }

    void SafetySupervisorNode::check_localization_staleness()
    {
        if (!last_odom_time_ns_.has_value())
        {
            return; // No odometry received yet
        }

        const std::uint64_t current_time_ns = now_ns();
        const std::uint64_t age_ns          = TimeUtils::calculate_age_ns(last_odom_time_ns_.value(), current_time_ns);

        (void)supervisor_->observe_localization_age(age_ns, localization_timeout_ns_);

        const Mode current_mode = machine_->mode();
        if (TimeUtils::is_stale(last_odom_time_ns_.value(), current_time_ns, localization_timeout_ns_) &&
            current_mode != Mode::LocalizationLost && current_mode != Mode::SafeStop)
        {
            (void)machine_->report_localization_lost();
        }
    }

    safety_core_msgs::msg::SafetyState SafetySupervisorNode::build_state_msg(Mode current_mode) const
    {
        safety_core_msgs::msg::SafetyState msg;
        msg.header.stamp        = get_clock()->now();
        msg.header.frame_id     = "base_link";
        msg.mode                = static_cast<std::uint8_t>(current_mode);
        msg.fault_latched       = machine_->fault_latched();
        msg.fault_code          = 0U; // Library does not currently expose code
        msg.zone.zone           = static_cast<std::uint8_t>(latest_zone_);
        msg.safe_stop_requested = safe_stop_requested_ || (current_mode == Mode::SafeStop);
        return msg;
    }

    void SafetySupervisorNode::publish_state(Mode current_mode)
    {
        state_pub_->publish(build_state_msg(current_mode));

        std_msgs::msg::Bool stop;
        stop.data = safe_stop_requested_ || (current_mode == Mode::SafeStop);
        safe_stop_pub_->publish(std::move(stop));
    }

    void SafetySupervisorNode::on_clear_fault(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
                                              std::shared_ptr<std_srvs::srv::Trigger::Response> response)
    {
        (void)request; // Unused
        const safety_core::Result result = machine_->clear_fault();
        if (result.ok())
        {
            safe_stop_requested_ = false;
            response->success    = true;
            response->message    = "Fault cleared successfully";
            RCLCPP_INFO(get_logger(), "Fault cleared via service call");
        }
        else
        {
            response->success = false;
            response->message = result.message;
            RCLCPP_WARN(get_logger(), "Failed to clear fault: %s", result.message.data());
        }
    }

} // namespace safety_core_ros
