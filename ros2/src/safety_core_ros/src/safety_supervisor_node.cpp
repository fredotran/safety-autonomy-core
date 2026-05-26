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
        params_.recovery_timeout_s         = ParamLoader::load_double(this, "recovery_timeout_s", 10.0);
        params_.recovery_speed_ramp_s      = ParamLoader::load_double(this, "recovery_speed_ramp_s", 5.0);
        params_.max_speed_mps              = ParamLoader::load_double(this, "max_speed_mps", 1.5);
        localization_timeout_ns_           = TimeUtils::seconds_to_nanoseconds(params_.localization_timeout_s);
        sensor_timeout_ns_                 = TimeUtils::seconds_to_nanoseconds(params_.sensor_timeout_s);
        degraded_recovery_ns_              = TimeUtils::seconds_to_nanoseconds(params_.degraded_recovery_s);
        recovery_timeout_ns_               = TimeUtils::seconds_to_nanoseconds(params_.recovery_timeout_s);
        recovery_speed_ramp_ns_            = TimeUtils::seconds_to_nanoseconds(params_.recovery_speed_ramp_s);
        current_speed_limit_mps_           = params_.max_speed_mps;
        target_speed_limit_mps_            = params_.max_speed_mps;

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

        if (params_.recovery_timeout_s <= 0.0)
        {
            RCLCPP_ERROR(get_logger(), "Invalid recovery_timeout_s: %.3f (must be positive)",
                         params_.recovery_timeout_s);
            throw std::runtime_error("recovery_timeout_s must be positive");
        }

        if (params_.recovery_speed_ramp_s <= 0.0)
        {
            RCLCPP_ERROR(get_logger(), "Invalid recovery_speed_ramp_s: %.3f (must be positive)",
                         params_.recovery_speed_ramp_s);
            throw std::runtime_error("recovery_speed_ramp_s must be positive");
        }

        if (params_.max_speed_mps <= 0.0)
        {
            RCLCPP_ERROR(get_logger(), "Invalid max_speed_mps: %.3f (must be positive)", params_.max_speed_mps);
            throw std::runtime_error("max_speed_mps must be positive");
        }

        // Create ROS interfaces with standardized QoS
        diag_pub_ =
            create_publisher<safety_core_msgs::msg::DiagnosticEvent>("safety/diagnostics", QosConfig::state_qos());
        state_pub_     = create_publisher<safety_core_msgs::msg::SafetyState>("safety/state", QosConfig::state_qos());
        safe_stop_pub_ = create_publisher<std_msgs::msg::Bool>("safety/safe_stop", QosConfig::state_qos());
        recovery_speed_limit_pub_ =
            create_publisher<std_msgs::msg::Float64>("safety/recovery_speed_limit", QosConfig::state_qos());

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
                    "degraded_recovery=%.3fs recovery_timeout=%.3fs recovery_speed_ramp=%.3fs max_speed=%.2f",
                    params_.localization_timeout_s, params_.sensor_timeout_s, params_.degraded_recovery_s,
                    params_.recovery_timeout_s, params_.recovery_speed_ramp_s, params_.max_speed_mps);
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

        // Check sensor health (summary-based transitions)
        check_sensor_health();

        // Check per-sensor recovery timeouts
        check_sensor_recovery_timeouts(current_time_ns);

        // Update ramped speed limit and publish
        update_speed_limit(current_time_ns);
        publish_recovery_speed_limit();

        // Observe clock sample
        (void)supervisor_->observe_clock_sample(current_time_ns);

        // Publish current state
        publish_state(current_mode);
    }

    void SafetySupervisorNode::on_sensor_health(const safety_core_msgs::msg::SensorHealth::ConstSharedPtr msg)
    {
        const std::string& name = msg->sensor_name;
        const uint8_t status    = msg->status;
        const std::uint64_t now = now_ns();

        if (name == "all")
        {
            worst_sensor_status_        = status;
            last_sensor_health_time_ns_ = now;
            return;
        }

        auto it = sensor_states_.find(name);
        if (it == sensor_states_.end())
        {
            SensorRecoveryInfo info;
            info.status = status;
            if (status == safety_core_msgs::msg::SensorHealth::DEGRADED)
            {
                info.degradation_start_ns = now;
            }
            sensor_states_.emplace(name, info);
            if (status == safety_core_msgs::msg::SensorHealth::DEGRADED)
            {
                handle_sensor_degraded(name);
            }
            return;
        }

        SensorRecoveryInfo& info = it->second;
        if (info.timed_out && status == safety_core_msgs::msg::SensorHealth::DEGRADED)
        {
            // Already timed out; ignore further degraded messages until sensor recovers
            return;
        }

        const uint8_t prev_status = info.status;
        if (prev_status == status)
        {
            return;
        }

        info.status = status;

        if (status == safety_core_msgs::msg::SensorHealth::DEGRADED)
        {
            info.degradation_start_ns = now;
            handle_sensor_degraded(name);
        }
        else if (prev_status == safety_core_msgs::msg::SensorHealth::DEGRADED &&
                 status == safety_core_msgs::msg::SensorHealth::HEALTHY)
        {
            info.degradation_start_ns = std::nullopt;
            handle_sensor_recovered(name);
        }
        else if (status == safety_core_msgs::msg::SensorHealth::FAULT)
        {
            info.degradation_start_ns = std::nullopt;
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

    void SafetySupervisorNode::handle_sensor_degraded(const std::string& sensor_name)
    {
        RCLCPP_WARN(get_logger(), "Sensor '%s' degraded - reducing max speed to 50%%", sensor_name.c_str());
        publish_diagnostic_event("safety/sensor_recovery", "Sensor " + sensor_name + " degraded");

        const Mode current_mode = machine_->mode();
        if (current_mode == Mode::Moving)
        {
            (void)machine_->transition_to(Mode::Degraded);
        }
        in_degraded_mode_        = true;
        target_speed_limit_mps_  = params_.max_speed_mps * 0.5;
        speed_ramp_start_ns_     = std::nullopt;
        current_speed_limit_mps_ = target_speed_limit_mps_;
    }

    void SafetySupervisorNode::handle_sensor_recovered(const std::string& sensor_name)
    {
        if (!is_any_sensor_degraded())
        {
            RCLCPP_INFO(get_logger(), "Sensor '%s' recovered - starting speed ramp", sensor_name.c_str());
            publish_diagnostic_event("safety/sensor_recovery", "Sensor " + sensor_name + " recovered, ramping speed");

            const Mode current_mode = machine_->mode();
            if (current_mode == Mode::Degraded)
            {
                (void)machine_->transition_to(Mode::Moving);
            }
            in_degraded_mode_       = false;
            target_speed_limit_mps_ = params_.max_speed_mps;
            speed_ramp_start_ns_    = now_ns();
        }
        else
        {
            RCLCPP_INFO(get_logger(), "Sensor '%s' recovered but other sensors still degraded", sensor_name.c_str());
            publish_diagnostic_event("safety/sensor_recovery",
                                     "Sensor " + sensor_name + " recovered but others degraded");
        }
    }

    bool SafetySupervisorNode::is_any_sensor_degraded() const noexcept
    {
        for (const auto& pair : sensor_states_)
        {
            if (pair.second.status == safety_core_msgs::msg::SensorHealth::DEGRADED)
            {
                return true;
            }
        }
        return false;
    }

    void SafetySupervisorNode::check_sensor_recovery_timeouts(std::uint64_t now_ns)
    {
        const Mode current_mode = machine_->mode();
        if (current_mode == Mode::SafeStop)
        {
            return;
        }

        for (auto& pair : sensor_states_)
        {
            SensorRecoveryInfo& info = pair.second;
            if (info.status != safety_core_msgs::msg::SensorHealth::DEGRADED || !info.degradation_start_ns.has_value())
            {
                continue;
            }

            const std::uint64_t elapsed_ns = now_ns - info.degradation_start_ns.value();
            if (elapsed_ns > recovery_timeout_ns_)
            {
                RCLCPP_ERROR(get_logger(), "Sensor '%s' recovery timeout exceeded (%.1fs) - treating as FAULT",
                             pair.first.c_str(), TimeUtils::nanoseconds_to_seconds(elapsed_ns));
                publish_diagnostic_event("safety/sensor_recovery",
                                         "Sensor " + pair.first + " recovery timeout, treating as FAULT");

                info.timed_out            = true;
                info.degradation_start_ns = std::nullopt;

                if (current_mode != Mode::SafeStop)
                {
                    (void)machine_->latch_fault(0xE003U); // Fault code for recovery timeout
                    (void)machine_->transition_to(Mode::SafeStop);
                    safe_stop_requested_ = true;
                    in_degraded_mode_    = false;
                }
            }
        }
    }

    void SafetySupervisorNode::update_speed_limit(std::uint64_t now_ns)
    {
        if (machine_->mode() == Mode::SafeStop)
        {
            current_speed_limit_mps_ = 0.0;
            target_speed_limit_mps_  = 0.0;
            speed_ramp_start_ns_     = std::nullopt;
            return;
        }

        if (speed_ramp_start_ns_.has_value())
        {
            const std::uint64_t elapsed_ns = now_ns - speed_ramp_start_ns_.value();
            if (elapsed_ns >= recovery_speed_ramp_ns_)
            {
                current_speed_limit_mps_ = target_speed_limit_mps_;
                speed_ramp_start_ns_     = std::nullopt;
            }
            else
            {
                const double degraded_limit = params_.max_speed_mps * 0.5;
                const double ramp_range     = params_.max_speed_mps - degraded_limit;
                const double fraction = static_cast<double>(elapsed_ns) / static_cast<double>(recovery_speed_ramp_ns_);
                current_speed_limit_mps_ = degraded_limit + (ramp_range * fraction);
            }
        }
        else
        {
            current_speed_limit_mps_ = target_speed_limit_mps_;
        }
    }

    void SafetySupervisorNode::publish_diagnostic_event(const std::string& topic, const std::string& payload)
    {
        safety_core_msgs::msg::DiagnosticEvent event;
        event.header.stamp        = get_clock()->now();
        event.topic               = topic;
        event.payload             = payload;
        event.topic_truncated     = (topic.size() > 255);
        event.payload_truncated   = (payload.size() > 1024);
        event.source_timestamp_ns = now_ns();
        diag_pub_->publish(std::move(event));
    }

    void SafetySupervisorNode::publish_recovery_speed_limit()
    {
        std_msgs::msg::Float64 msg;
        msg.data = current_speed_limit_mps_;
        recovery_speed_limit_pub_->publish(std::move(msg));
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
