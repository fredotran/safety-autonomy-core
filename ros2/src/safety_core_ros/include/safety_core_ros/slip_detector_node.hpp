// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include "safety_core_ros/param_loader.hpp"
#include "safety_core_ros/qos_config.hpp"

#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <safety_core_msgs/msg/sensor_health.hpp>
#include <safety_core_msgs/msg/slip_detected.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <string>

namespace safety_core_ros
{

    /**
     * @brief Wheel slip detector node
     *
     * Subscribes to wheel odometry (/odom) and IMU (/imu), compares wheel
     * velocities with IMU angular velocity and acceleration-integrated velocity
     * estimates, and publishes slip detection results.
     *
     * Outputs:
     *   - /safety/slip_detected : safety_core_msgs::msg::SlipDetected
     *   - /safety/slip_status   : safety_core_msgs::msg::SensorHealth
     */
    class SlipDetectorNode : public rclcpp::Node
    {
      public:
        explicit SlipDetectorNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions{});

      private:
        void on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg);
        void on_imu(const sensor_msgs::msg::Imu::ConstSharedPtr msg);
        void timer_tick();

        void update_imu_velocity_estimate(const sensor_msgs::msg::Imu& msg, double dt);
        safety_core_msgs::msg::SlipDetected build_slip_msg(double linear_diff, double angular_diff, bool is_slipping);
        safety_core_msgs::msg::SensorHealth build_health_msg(bool is_slipping, const std::string& detail);

        [[nodiscard]] double now_seconds() const;

        // Parameters
        struct Params
        {
            double slip_threshold_mps{0.1};
            double angular_slip_threshold_radps{0.3};
            double imu_vel_decay_time_s{2.0};
            double publish_period_s{0.1};
            double max_imu_age_s{0.05};
            double stationary_threshold_mps{0.01};
        } params_;

        // State
        std::mutex mutex_;
        nav_msgs::msg::Odometry::ConstSharedPtr latest_odom_;
        sensor_msgs::msg::Imu::ConstSharedPtr latest_imu_;

        // Simple IMU-integrated forward velocity estimate
        double imu_vel_x_{0.0};
        rclcpp::Time last_imu_time_{0, 0, RCL_ROS_TIME};
        bool have_imu_time_{false};

        std::atomic<bool> is_slipping_{false};
        std::atomic<double> last_slip_magnitude_{0.0};
        std::atomic<double> last_slip_ratio_{0.0};
        std::string last_detail_;

        // ROS interfaces
        rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
        rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
        rclcpp::Publisher<safety_core_msgs::msg::SlipDetected>::SharedPtr slip_pub_;
        rclcpp::Publisher<safety_core_msgs::msg::SensorHealth>::SharedPtr health_pub_;
        rclcpp::TimerBase::SharedPtr timer_;
    };

} // namespace safety_core_ros
