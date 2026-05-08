#pragma once

#include "safety_core/motion/trajectory.hpp"
#include "safety_core/platform/actuators/drive_actuator.hpp"
#include "safety_core_ros/math_utils.hpp"
#include "safety_core_ros/param_loader.hpp"
#include "safety_core_ros/qos_config.hpp"
#include "safety_core_ros/time_utils.hpp"

#include <atomic>
#include <cstdint>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <safety_core_msgs/msg/envelope_status.hpp>
#include <safety_core_msgs/msg/safety_state.hpp>
#include <std_msgs/msg/bool.hpp>

namespace safety_core_ros
{

    /**
     * @brief Safety drive bridge node for command filtering and safe-stop execution
     *
     * Sits between Nav2 (publishes /cmd_vel_nav) and Gazebo's diff-drive plugin
     * (subscribes /cmd_vel). Applies the safety_core motion envelope:
     *   - Clamp linear/angular based on EnvelopeStatus.recommended_speed_limit
     *   - Detect stale Nav2 commands via DriveActuator::is_command_fresh()
     *   - Generate a jerk-limited deceleration ramp on safe-stop assertion
     *   - Never publish if the upstream supervisor latched a fault
     *
     * The bridge ensures that all commands respect safety constraints and
     * provides smooth, controlled stopping when safety conditions are violated.
     */
    class SafetyDriveBridgeNode : public rclcpp::Node
    {
      public:
        explicit SafetyDriveBridgeNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions{});

      private:
        // Callbacks
        void on_cmd_vel_nav(const geometry_msgs::msg::Twist::ConstSharedPtr msg);
        void on_envelope(const safety_core_msgs::msg::EnvelopeStatus::ConstSharedPtr msg);
        void on_safe_stop(const std_msgs::msg::Bool::ConstSharedPtr msg);
        void on_state(const safety_core_msgs::msg::SafetyState::ConstSharedPtr msg);
        void on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg);

        // Control loop
        void control_tick();

        // Command processing
        void process_and_publish_command();
        geometry_msgs::msg::Twist clamp_command(const geometry_msgs::msg::Twist& cmd, double speed_limit);
        void publish_zero_twist();

        // Safe-stop handling
        void start_jerk_limited_stop();
        bool execute_stop_profile();

        // Time utilities
        [[nodiscard]] std::uint64_t now_ns() const noexcept;

        // Configuration
        struct Params
        {
            double max_linear_mps{1.5};
            double max_angular_radps{1.5};
            double max_decel_mps2{1.0};
            double max_jerk_mps3{2.0};
            double cmd_freshness_s{0.25};
            double control_period_s{0.05};
        } params_;

        // State
        std::atomic<bool> safe_stop_active_{false};
        std::atomic<bool> fault_latched_{false};
        std::atomic<double> recommended_speed_limit_mps_{1e9}; // unrestricted by default
        std::atomic<double> measured_forward_speed_mps_{0.0};

        // Latest Nav2 command + arrival timestamp
        std::optional<geometry_msgs::msg::Twist> latest_nav_cmd_;
        std::uint64_t latest_nav_cmd_time_ns_{0U};
        std::uint64_t cmd_freshness_timeout_ns_{250'000'000ULL}; // 0.25s in ns

        // Jerk-limited stop state
        bool stop_profile_active_{false};
        std::size_t stop_profile_index_{0U};
        std::size_t stop_profile_count_{0U};
        static constexpr std::size_t kStopProfileCapacity = 256U;
        safety_core::motion::TrajectoryPoint stop_profile_[kStopProfileCapacity]{};
        double last_published_linear_{0.0};

        // ROS interfaces
        rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_nav_sub_;
        rclcpp::Subscription<safety_core_msgs::msg::EnvelopeStatus>::SharedPtr envelope_sub_;
        rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr safe_stop_sub_;
        rclcpp::Subscription<safety_core_msgs::msg::SafetyState>::SharedPtr state_sub_;
        rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
        rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
        rclcpp::TimerBase::SharedPtr timer_;
    };

} // namespace safety_core_ros
