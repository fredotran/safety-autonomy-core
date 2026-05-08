#pragma once

#include "safety_core/motion/trajectory.hpp"
#include "safety_core/platform/actuators/drive_actuator.hpp"

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

    // Sits between Nav2 (publishes /cmd_vel_nav) and Gazebo's diff-drive plugin
    // (subscribes /cmd_vel). Applies the safety_core motion envelope:
    //   * clamp linear/angular based on EnvelopeStatus.recommended_speed_limit
    //   * detect stale Nav2 commands via DriveActuator::is_command_fresh()
    //   * generate a jerk-limited deceleration ramp on safe-stop assertion
    //   * never publish if the upstream supervisor latched a fault
    class SafetyDriveBridgeNode : public rclcpp::Node
    {
      public:
        explicit SafetyDriveBridgeNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions{});

      private:
        void declare_params();

        void on_cmd_vel_nav(const geometry_msgs::msg::Twist::ConstSharedPtr msg);
        void on_envelope(const safety_core_msgs::msg::EnvelopeStatus::ConstSharedPtr msg);
        void on_safe_stop(const std_msgs::msg::Bool::ConstSharedPtr msg);
        void on_state(const safety_core_msgs::msg::SafetyState::ConstSharedPtr msg);
        void on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg);

        void control_tick();
        void publish_zero_twist();
        void start_jerk_limited_stop();

        [[nodiscard]] std::uint64_t now_ns() const noexcept;

        // Parameters
        double max_linear_mps_{1.5};
        double max_angular_radps_{1.5};
        double max_decel_mps2_{1.0};
        double max_jerk_mps3_{2.0};
        double cmd_freshness_s_{0.25};
        double control_period_s_{0.05};

        // State
        std::atomic<bool> safe_stop_active_{false};
        std::atomic<bool> fault_latched_{false};
        std::atomic<double> recommended_speed_limit_mps_{1e9}; // unrestricted by default
        std::atomic<double> measured_forward_speed_mps_{0.0};

        // Latest Nav2 command + arrival timestamp
        std::optional<geometry_msgs::msg::Twist> latest_nav_cmd_;
        std::uint64_t latest_nav_cmd_time_ns_{0U};

        // Jerk-limited stop state
        bool stop_profile_active_{false};
        std::size_t stop_profile_index_{0U};
        std::size_t stop_profile_count_{0U};
        static constexpr std::size_t kStopProfileCapacity = 256U;
        safety_core::motion::TrajectoryPoint stop_profile_[kStopProfileCapacity]{};
        double last_published_linear_{0.0};

        rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_nav_sub_;
        rclcpp::Subscription<safety_core_msgs::msg::EnvelopeStatus>::SharedPtr envelope_sub_;
        rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr safe_stop_sub_;
        rclcpp::Subscription<safety_core_msgs::msg::SafetyState>::SharedPtr state_sub_;
        rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
        rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
        rclcpp::TimerBase::SharedPtr timer_;
    };

} // namespace safety_core_ros
