// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/safety_supervisor_node.hpp"

#include <chrono>

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
        localization_timeout_ns_           = TimeUtils::seconds_to_nanoseconds(params_.localization_timeout_s);

        // Create ROS interfaces with standardized QoS
        diag_pub_ =
            create_publisher<safety_core_msgs::msg::DiagnosticEvent>("safety/diagnostics", QosConfig::state_qos());
        state_pub_     = create_publisher<safety_core_msgs::msg::SafetyState>("safety/state", QosConfig::state_qos());
        safe_stop_pub_ = create_publisher<std_msgs::msg::Bool>("safety/safe_stop", QosConfig::state_qos());

        envelope_sub_ = create_subscription<safety_core_msgs::msg::EnvelopeStatus>(
            "safety/envelope_status", QosConfig::sensor_qos(), std::bind(&SafetySupervisorNode::on_envelope, this, _1));
        odom_sub_ = create_subscription<nav_msgs::msg::Odometry>("odom", QosConfig::sensor_qos(),
                                                                 std::bind(&SafetySupervisorNode::on_odom, this, _1));

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

        RCLCPP_INFO(get_logger(), "safety_supervisor_node initialized | localization_timeout=%.3fs",
                    params_.localization_timeout_s);
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

        // Observe clock sample
        (void)supervisor_->observe_clock_sample(current_time_ns);

        // Publish current state
        publish_state(current_mode);
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
