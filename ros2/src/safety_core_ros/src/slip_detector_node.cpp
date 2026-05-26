// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/slip_detector_node.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

using namespace std::chrono_literals;
using std::placeholders::_1;

namespace safety_core_ros
{

    SlipDetectorNode::SlipDetectorNode(const rclcpp::NodeOptions& options) : rclcpp::Node("slip_detector_node", options)
    {
        // Load parameters
        params_.slip_threshold_mps           = ParamLoader::load_double(this, "slip_threshold_mps", 0.1);
        params_.angular_slip_threshold_radps = ParamLoader::load_double(this, "angular_slip_threshold_radps", 0.3);
        params_.imu_vel_decay_time_s         = ParamLoader::load_double(this, "imu_vel_decay_time_s", 2.0);
        params_.publish_period_s             = ParamLoader::load_double(this, "publish_period_s", 0.1);
        params_.max_imu_age_s                = ParamLoader::load_double(this, "max_imu_age_s", 0.05);
        params_.stationary_threshold_mps     = ParamLoader::load_double(this, "stationary_threshold_mps", 0.01);

        // Validate parameters
        if (params_.slip_threshold_mps < 0.0)
        {
            RCLCPP_WARN(get_logger(), "slip_threshold_mps negative (%.3f), clamping to 0.0",
                        params_.slip_threshold_mps);
            params_.slip_threshold_mps = 0.0;
        }
        if (params_.angular_slip_threshold_radps < 0.0)
        {
            RCLCPP_WARN(get_logger(), "angular_slip_threshold_radps negative (%.3f), clamping to 0.0",
                        params_.angular_slip_threshold_radps);
            params_.angular_slip_threshold_radps = 0.0;
        }
        if (params_.imu_vel_decay_time_s <= 0.0)
        {
            RCLCPP_ERROR(get_logger(), "imu_vel_decay_time_s must be positive: %.3f", params_.imu_vel_decay_time_s);
            throw std::runtime_error("imu_vel_decay_time_s must be positive");
        }
        if (params_.publish_period_s <= 0.0)
        {
            RCLCPP_ERROR(get_logger(), "publish_period_s must be positive: %.3f", params_.publish_period_s);
            throw std::runtime_error("publish_period_s must be positive");
        }

        // Create ROS interfaces
        odom_sub_ = create_subscription<nav_msgs::msg::Odometry>("odom", QosConfig::sensor_qos(),
                                                                 std::bind(&SlipDetectorNode::on_odom, this, _1));
        imu_sub_  = create_subscription<sensor_msgs::msg::Imu>("imu", QosConfig::sensor_qos(),
                                                               std::bind(&SlipDetectorNode::on_imu, this, _1));

        slip_pub_ =
            create_publisher<safety_core_msgs::msg::SlipDetected>("safety/slip_detected", QosConfig::state_qos());
        health_pub_ =
            create_publisher<safety_core_msgs::msg::SensorHealth>("safety/slip_status", QosConfig::state_qos());

        // Create timer for periodic publishing
        timer_ = create_wall_timer(std::chrono::duration<double>(params_.publish_period_s),
                                   std::bind(&SlipDetectorNode::timer_tick, this));

        RCLCPP_INFO(get_logger(),
                    "slip_detector_node initialized | slip_threshold=%.3f m/s angular_threshold=%.3f rad/s "
                    "imu_decay=%.1f s publish_period=%.3f s",
                    params_.slip_threshold_mps, params_.angular_slip_threshold_radps, params_.imu_vel_decay_time_s,
                    params_.publish_period_s);
    }

    double SlipDetectorNode::now_seconds() const
    {
        return get_clock()->now().seconds();
    }

    void SlipDetectorNode::update_imu_velocity_estimate(const sensor_msgs::msg::Imu& msg, double dt)
    {
        if (dt <= 0.0 || dt > 0.5)
        {
            return; // reject unreasonable dt
        }

        // Integrate forward acceleration (x-axis in body frame) to estimate velocity
        imu_vel_x_ += msg.linear_acceleration.x * dt;

        // Apply exponential decay to prevent drift buildup
        const double decay = std::exp(-dt / params_.imu_vel_decay_time_s);
        imu_vel_x_ *= decay;
    }

    void SlipDetectorNode::on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        latest_odom_ = msg;
    }

    void SlipDetectorNode::on_imu(const sensor_msgs::msg::Imu::ConstSharedPtr msg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        latest_imu_ = msg;

        if (have_imu_time_)
        {
            const double dt = (rclcpp::Time(msg->header.stamp) - last_imu_time_).seconds();
            update_imu_velocity_estimate(*msg, dt);
        }

        last_imu_time_ = rclcpp::Time(msg->header.stamp);
        have_imu_time_ = true;
    }

    safety_core_msgs::msg::SlipDetected SlipDetectorNode::build_slip_msg(double linear_diff, double angular_diff,
                                                                         bool is_slipping)
    {
        safety_core_msgs::msg::SlipDetected msg;
        msg.header.stamp    = get_clock()->now();
        msg.header.frame_id = "base_link";
        msg.is_slipping     = is_slipping;

        double slip_magnitude = 0.0;
        double slip_ratio     = 0.0;
        std::string detail;

        if (is_slipping)
        {
            // Determine which type of slip dominates
            const bool linear_slip  = linear_diff > params_.slip_threshold_mps;
            const bool angular_slip = angular_diff > params_.angular_slip_threshold_radps;

            if (linear_slip && angular_slip)
            {
                slip_magnitude = std::max(linear_diff, angular_diff);
                detail         = "combined slip";
            }
            else if (linear_slip)
            {
                slip_magnitude = linear_diff;
                detail         = "linear slip";
            }
            else
            {
                slip_magnitude = angular_diff;
                detail         = "angular slip";
            }

            // Compute slip ratio as percentage of expected velocity
            double expected_speed = 0.0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (latest_odom_ != nullptr)
                {
                    expected_speed = std::abs(latest_odom_->twist.twist.linear.x);
                }
            }

            if (expected_speed > params_.stationary_threshold_mps)
            {
                slip_ratio = (slip_magnitude / expected_speed) * 100.0;
            }
            else
            {
                // Robot is effectively stationary but sensors disagree
                slip_ratio = 100.0;
            }
        }
        else
        {
            slip_magnitude = 0.0;
            slip_ratio     = 0.0;
            detail         = "no slip";
        }

        msg.slip_magnitude = static_cast<float>(slip_magnitude);
        msg.slip_ratio     = static_cast<float>(slip_ratio);
        msg.detail         = detail;

        return msg;
    }

    safety_core_msgs::msg::SensorHealth SlipDetectorNode::build_health_msg(bool is_slipping, const std::string& detail)
    {
        safety_core_msgs::msg::SensorHealth msg;
        msg.header.stamp     = get_clock()->now();
        msg.header.frame_id  = "slip_detector";
        msg.sensor_name      = "wheel_odometry";
        msg.expected_rate_hz = 0.0; // N/A for slip detector
        msg.update_rate_hz   = 0.0;
        msg.data_age_s       = 0.0;

        if (is_slipping)
        {
            msg.status     = safety_core_msgs::msg::SensorHealth::DEGRADED;
            msg.error_rate = 1.0;
            msg.detail     = detail.empty() ? "Wheel slip detected" : detail;
        }
        else
        {
            msg.status     = safety_core_msgs::msg::SensorHealth::HEALTHY;
            msg.error_rate = 0.0;
            msg.detail     = "Wheel odometry consistent with IMU";
        }

        return msg;
    }

    void SlipDetectorNode::timer_tick()
    {
        nav_msgs::msg::Odometry::ConstSharedPtr odom;
        sensor_msgs::msg::Imu::ConstSharedPtr imu;
        double current_imu_vel_x = 0.0;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            odom              = latest_odom_;
            imu               = latest_imu_;
            current_imu_vel_x = imu_vel_x_;
        }

        if (odom == nullptr)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "No odometry received yet");
            return;
        }

        if (imu == nullptr)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "No IMU data received yet");
            return;
        }

        // Check IMU age
        const double odom_time = odom->header.stamp.sec + odom->header.stamp.nanosec * 1e-9;
        const double imu_time  = imu->header.stamp.sec + imu->header.stamp.nanosec * 1e-9;
        const double imu_age   = std::abs(odom_time - imu_time);

        if (imu_age > params_.max_imu_age_s)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
                                 "IMU data too old (age=%.3f s, max=%.3f s), skipping slip detection", imu_age,
                                 params_.max_imu_age_s);
            return;
        }

        // Extract wheel velocities
        const double wheel_linear_x  = odom->twist.twist.linear.x;
        const double wheel_angular_z = odom->twist.twist.angular.z;
        const double imu_angular_z   = imu->angular_velocity.z;

        // Compute differences
        const double linear_diff  = std::abs(wheel_linear_x - current_imu_vel_x);
        const double angular_diff = std::abs(wheel_angular_z - imu_angular_z);

        // Detect slip
        const bool linear_slip  = linear_diff > params_.slip_threshold_mps;
        const bool angular_slip = angular_diff > params_.angular_slip_threshold_radps;
        const bool is_slipping  = linear_slip || angular_slip;

        // Build and publish SlipDetected message
        auto slip_msg = build_slip_msg(linear_diff, angular_diff, is_slipping);
        slip_pub_->publish(std::move(slip_msg));

        // Build and publish SensorHealth message
        std::string detail;
        if (is_slipping)
        {
            if (linear_slip && angular_slip)
            {
                detail = "combined slip (linear: " + std::to_string(linear_diff) +
                         " m/s, angular: " + std::to_string(angular_diff) + " rad/s)";
            }
            else if (linear_slip)
            {
                detail = "linear slip (diff: " + std::to_string(linear_diff) + " m/s)";
            }
            else
            {
                detail = "angular slip (diff: " + std::to_string(angular_diff) + " rad/s)";
            }
        }
        auto health_msg = build_health_msg(is_slipping, detail);
        health_pub_->publish(std::move(health_msg));

        // Update atomic state for introspection
        is_slipping_.store(is_slipping, std::memory_order_relaxed);
        last_slip_magnitude_.store(static_cast<double>(slip_msg.slip_magnitude), std::memory_order_relaxed);
        last_slip_ratio_.store(static_cast<double>(slip_msg.slip_ratio), std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_detail_ = detail;
        }

        if (is_slipping)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
                                 "Wheel slip detected | linear_diff=%.3f m/s angular_diff=%.3f rad/s detail=%s",
                                 linear_diff, angular_diff, detail.c_str());
        }
    }

} // namespace safety_core_ros
