#include "safety_core_ros/safety_drive_bridge_node.hpp"

#include "safety_core/motion/trajectory.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

using std::placeholders::_1;

namespace safety_core_ros
{

    SafetyDriveBridgeNode::SafetyDriveBridgeNode(const rclcpp::NodeOptions& options)
        : rclcpp::Node("safety_drive_bridge_node", options)
    {
        // Load parameters
        params_.max_linear_mps    = ParamLoader::load_double(this, "max_linear_mps", 1.5);
        params_.max_angular_radps = ParamLoader::load_double(this, "max_angular_radps", 1.5);
        params_.max_decel_mps2    = ParamLoader::load_double(this, "max_decel_mps2", 1.0);
        params_.max_jerk_mps3     = ParamLoader::load_double(this, "max_jerk_mps3", 2.0);
        params_.cmd_freshness_s   = ParamLoader::load_double(this, "cmd_freshness_s", 0.25);
        params_.control_period_s  = ParamLoader::load_double(this, "control_period_s", 0.05);

        cmd_freshness_timeout_ns_ = TimeUtils::seconds_to_nanoseconds(params_.cmd_freshness_s);

        // Create ROS interfaces with standardized QoS
        cmd_vel_nav_sub_ = create_subscription<geometry_msgs::msg::Twist>(
            "cmd_vel_nav", QosConfig::state_qos(), std::bind(&SafetyDriveBridgeNode::on_cmd_vel_nav, this, _1));
        envelope_sub_ = create_subscription<safety_core_msgs::msg::EnvelopeStatus>(
            "safety/envelope_status", QosConfig::state_qos(), std::bind(&SafetyDriveBridgeNode::on_envelope, this, _1));
        safe_stop_sub_ = create_subscription<std_msgs::msg::Bool>(
            "safety/safe_stop", QosConfig::state_qos(), std::bind(&SafetyDriveBridgeNode::on_safe_stop, this, _1));
        state_sub_ = create_subscription<safety_core_msgs::msg::SafetyState>(
            "safety/state", QosConfig::state_qos(), std::bind(&SafetyDriveBridgeNode::on_state, this, _1));
        odom_sub_ = create_subscription<nav_msgs::msg::Odometry>("odom", QosConfig::sensor_qos(),
                                                                 std::bind(&SafetyDriveBridgeNode::on_odom, this, _1));

        cmd_vel_pub_ = create_publisher<geometry_msgs::msg::Twist>("cmd_vel", QosConfig::state_qos());

        const auto period = std::chrono::duration<double>(params_.control_period_s);
        timer_            = create_wall_timer(period, std::bind(&SafetyDriveBridgeNode::control_tick, this));

        RCLCPP_INFO(get_logger(),
                    "safety_drive_bridge_node initialized | linear<=%.2fm/s ang<=%.2frad/s | "
                    "decel=%.2fm/s^2 jerk=%.2fm/s^3 | freshness=%.3fs",
                    params_.max_linear_mps, params_.max_angular_radps, params_.max_decel_mps2, params_.max_jerk_mps3,
                    params_.cmd_freshness_s);
    }

    std::uint64_t SafetyDriveBridgeNode::now_ns() const noexcept
    {
        return TimeUtils::now_nanoseconds(get_clock());
    }

    void SafetyDriveBridgeNode::on_cmd_vel_nav(const geometry_msgs::msg::Twist::ConstSharedPtr msg)
    {
        latest_nav_cmd_         = *msg;
        latest_nav_cmd_time_ns_ = now_ns();
    }

    void SafetyDriveBridgeNode::on_envelope(const safety_core_msgs::msg::EnvelopeStatus::ConstSharedPtr msg)
    {
        // Treat 0 (or near-zero) recommended speed as "use parameter ceiling"
        // only when zone is Clear; otherwise honor the recommendation.
        const double recommended = msg->recommended_speed_limit_mps;
        if (msg->zone.zone == 0U /* CLEAR */)
        {
            recommended_speed_limit_mps_.store(params_.max_linear_mps, std::memory_order_relaxed);
        }
        else
        {
            recommended_speed_limit_mps_.store(std::max(0.0, recommended), std::memory_order_relaxed);
        }
    }

    void SafetyDriveBridgeNode::on_safe_stop(const std_msgs::msg::Bool::ConstSharedPtr msg)
    {
        const bool prev = safe_stop_active_.exchange(msg->data, std::memory_order_relaxed);
        if (msg->data && !prev)
        {
            start_jerk_limited_stop();
        }
        if (!msg->data)
        {
            stop_profile_active_ = false;
        }
    }

    void SafetyDriveBridgeNode::on_state(const safety_core_msgs::msg::SafetyState::ConstSharedPtr msg)
    {
        fault_latched_.store(msg->fault_latched, std::memory_order_relaxed);
    }

    void SafetyDriveBridgeNode::on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg)
    {
        measured_forward_speed_mps_.store(msg->twist.twist.linear.x, std::memory_order_relaxed);
    }

    void SafetyDriveBridgeNode::start_jerk_limited_stop()
    {
        const double speed = std::max(0.0, measured_forward_speed_mps_.load(std::memory_order_relaxed));
        const bool ok      = safety_core::motion::generate_jerk_limited_stop_profile(
            speed, params_.max_decel_mps2, params_.max_jerk_mps3, params_.control_period_s, stop_profile_,
            kStopProfileCapacity, stop_profile_count_);

        stop_profile_index_  = 0U;
        stop_profile_active_ = ok && (stop_profile_count_ > 0U);
        if (!ok)
        {
            RCLCPP_WARN(get_logger(), "[drive_bridge] jerk-limited stop profile generation failed; "
                                      "falling back to immediate zero twist");
        }
        else
        {
            RCLCPP_INFO(get_logger(), "[drive_bridge] jerk-limited stop engaged | %zu samples", stop_profile_count_);
        }
    }

    void SafetyDriveBridgeNode::publish_zero_twist()
    {
        geometry_msgs::msg::Twist cmd;
        cmd.linear.x  = 0.0;
        cmd.angular.z = 0.0;
        cmd_vel_pub_->publish(std::move(cmd));
        last_published_linear_ = 0.0;
    }

    geometry_msgs::msg::Twist SafetyDriveBridgeNode::clamp_command(const geometry_msgs::msg::Twist& cmd,
                                                                   double speed_limit)
    {
        geometry_msgs::msg::Twist clamped_cmd;
        const double v_limit  = std::min(params_.max_linear_mps, speed_limit);
        clamped_cmd.linear.x  = std::clamp(cmd.linear.x, -v_limit, v_limit);
        clamped_cmd.angular.z = std::clamp(cmd.angular.z, -params_.max_angular_radps, params_.max_angular_radps);
        return clamped_cmd;
    }

    bool SafetyDriveBridgeNode::execute_stop_profile()
    {
        if (stop_profile_active_ && stop_profile_index_ < stop_profile_count_)
        {
            geometry_msgs::msg::Twist cmd;
            cmd.linear.x           = stop_profile_[stop_profile_index_].speed_mps;
            cmd.angular.z          = 0.0;
            last_published_linear_ = cmd.linear.x;
            cmd_vel_pub_->publish(std::move(cmd));
            ++stop_profile_index_;
            return true;
        }
        return false;
    }

    void SafetyDriveBridgeNode::process_and_publish_command()
    {
        const double speed_limit      = recommended_speed_limit_mps_.load(std::memory_order_relaxed);
        geometry_msgs::msg::Twist cmd = clamp_command(*latest_nav_cmd_, speed_limit);
        cmd_vel_pub_->publish(std::move(cmd));
        last_published_linear_ = cmd.linear.x;
    }

    void SafetyDriveBridgeNode::control_tick()
    {
        // Cache atomic variables and current time to avoid repeated loads
        const bool fault_latched            = fault_latched_.load(std::memory_order_relaxed);
        const bool safe_stop_active         = safe_stop_active_.load(std::memory_order_relaxed);
        const std::uint64_t current_time_ns = now_ns();

        // 1. Hard fault latched: always stop
        if (fault_latched)
        {
            publish_zero_twist();
            return;
        }

        // 2. Safe-stop active: follow jerk-limited deceleration profile
        if (safe_stop_active)
        {
            if (!execute_stop_profile())
            {
                publish_zero_twist();
            }
            return;
        }

        // 3. No fresh Nav2 command: stop
        if (!latest_nav_cmd_.has_value())
        {
            publish_zero_twist();
            return;
        }

        // Check command staleness
        const std::uint64_t age_ns = TimeUtils::calculate_age_ns(latest_nav_cmd_time_ns_, current_time_ns);
        if (TimeUtils::is_stale(latest_nav_cmd_time_ns_, current_time_ns, cmd_freshness_timeout_ns_))
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
                                 "[drive_bridge] Nav2 command stale (age=%.3fs, limit=%.3fs); zeroing",
                                 TimeUtils::nanoseconds_to_seconds(age_ns),
                                 TimeUtils::nanoseconds_to_seconds(cmd_freshness_timeout_ns_));
            publish_zero_twist();
            return;
        }

        // 4. Apply safety envelope clamp and publish
        process_and_publish_command();
    }

} // namespace safety_core_ros
