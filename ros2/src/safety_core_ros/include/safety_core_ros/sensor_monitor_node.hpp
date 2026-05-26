// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include "safety_core_ros/param_loader.hpp"
#include "safety_core_ros/qos_config.hpp"
#include "safety_core_ros/time_utils.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/time.hpp>
#include <safety_core_msgs/msg/sensor_health.hpp>
#include <safety_core_msgs/msg/sensor_health_metrics.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <string>

namespace safety_core_ros
{

    /**
     * @brief Sensor health monitor node
     *
     * Monitors incoming sensor streams (/scan, /imu, /gps, /odom) and publishes
     * per-sensor health diagnostics on /safety/sensor_health. Each sensor is
     * evaluated for update rate, data age, and dropout rate against configured
     * expected frequencies. An aggregate "all" summary is published alongside
     * the individual sensor reports.
     */
    class SensorMonitorNode : public rclcpp::Node
    {
      public:
        explicit SensorMonitorNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions{});

      private:
        struct SensorTracker
        {
            struct StatusSample
            {
                std::uint64_t timestamp_ns;
                uint8_t status;
                double update_rate_hz;
            };

            std::string sensor_name;
            double expected_rate_hz{0.0};
            std::atomic<std::uint64_t> last_msg_time_ns{0};
            std::deque<std::uint64_t> msg_times;
            std::deque<StatusSample> status_history;
            std::mutex mutex;
        };

        void on_scan(const sensor_msgs::msg::LaserScan::ConstSharedPtr msg);
        void on_imu(const sensor_msgs::msg::Imu::ConstSharedPtr msg);
        void on_gps(const sensor_msgs::msg::NavSatFix::ConstSharedPtr msg);
        void on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg);
        void timer_tick();

        void record_message(SensorTracker& tracker, std::uint64_t now_ns);
        void prune_window(SensorTracker& tracker, std::uint64_t now_ns, double window_s);
        safety_core_msgs::msg::SensorHealth build_sensor_msg(SensorTracker& tracker, std::uint64_t now_ns);
        void publish_sensor_health(SensorTracker& tracker, std::uint64_t now_ns);
        void record_status_sample(SensorTracker& tracker, std::uint64_t now_ns, uint8_t status, double update_rate_hz);
        void prune_status_history(SensorTracker& tracker, std::uint64_t now_ns);
        safety_core_msgs::msg::SensorHealthMetrics build_metrics_msg(SensorTracker& tracker, std::uint64_t now_ns);
        void publish_metrics();

        [[nodiscard]] std::uint64_t now_ns() const noexcept;

        // Parameters
        struct Params
        {
            double lidar_expected_rate{10.0};
            double imu_expected_rate{100.0};
            double gps_expected_rate{10.0};
            double odometry_expected_rate{50.0};
            double publish_period_s{1.0};
            double healthy_threshold{0.8};
            double degraded_threshold{0.5};
            double metrics_window_s{60.0};
        } params_;

        // Sensor trackers
        SensorTracker lidar_tracker_;
        SensorTracker imu_tracker_;
        SensorTracker gps_tracker_;
        SensorTracker odom_tracker_;

        // Startup grace period
        rclcpp::Time startup_time_;
        static constexpr double kStartupGracePeriodS = 2.0;

        // ROS interfaces
        rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;
        rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
        rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gps_sub_;
        rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
        rclcpp::Publisher<safety_core_msgs::msg::SensorHealth>::SharedPtr health_pub_;
        rclcpp::Publisher<safety_core_msgs::msg::SensorHealthMetrics>::SharedPtr metrics_pub_;
        rclcpp::TimerBase::SharedPtr timer_;
        rclcpp::TimerBase::SharedPtr metrics_timer_;
    };

} // namespace safety_core_ros
