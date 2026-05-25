// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/sensor_monitor_node.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace std::chrono_literals;
using std::placeholders::_1;

namespace safety_core_ros
{

    SensorMonitorNode::SensorMonitorNode(const rclcpp::NodeOptions& options)
        : rclcpp::Node("sensor_monitor_node", options)
    {
        // Load parameters
        params_.lidar_expected_rate    = ParamLoader::load_double(this, "lidar_expected_rate", 10.0);
        params_.imu_expected_rate      = ParamLoader::load_double(this, "imu_expected_rate", 100.0);
        params_.gps_expected_rate      = ParamLoader::load_double(this, "gps_expected_rate", 10.0);
        params_.odometry_expected_rate = ParamLoader::load_double(this, "odometry_expected_rate", 50.0);
        params_.publish_period_s       = ParamLoader::load_double(this, "publish_period_s", 1.0);
        params_.healthy_threshold      = ParamLoader::load_double(this, "healthy_threshold", 0.8);
        params_.degraded_threshold     = ParamLoader::load_double(this, "degraded_threshold", 0.5);

        // Validate parameters
        if (params_.publish_period_s <= 0.0)
        {
            RCLCPP_ERROR(get_logger(), "Invalid publish_period_s: %.3f (must be positive)", params_.publish_period_s);
            throw std::runtime_error("publish_period_s must be positive");
        }

        if (params_.healthy_threshold < 0.0 || params_.healthy_threshold > 1.0)
        {
            RCLCPP_WARN(get_logger(), "healthy_threshold out of range [0,1]: %.2f, clamping",
                        params_.healthy_threshold);
            params_.healthy_threshold = std::clamp(params_.healthy_threshold, 0.0, 1.0);
        }

        if (params_.degraded_threshold < 0.0 || params_.degraded_threshold > 1.0)
        {
            RCLCPP_WARN(get_logger(), "degraded_threshold out of range [0,1]: %.2f, clamping",
                        params_.degraded_threshold);
            params_.degraded_threshold = std::clamp(params_.degraded_threshold, 0.0, 1.0);
        }

        if (params_.degraded_threshold >= params_.healthy_threshold)
        {
            RCLCPP_WARN(get_logger(),
                        "degraded_threshold (%.2f) >= healthy_threshold (%.2f), this may cause unexpected behavior",
                        params_.degraded_threshold, params_.healthy_threshold);
        }

        // Initialize trackers
        lidar_tracker_.sensor_name      = "lidar";
        lidar_tracker_.expected_rate_hz = params_.lidar_expected_rate;
        imu_tracker_.sensor_name        = "imu";
        imu_tracker_.expected_rate_hz   = params_.imu_expected_rate;
        gps_tracker_.sensor_name        = "gps";
        gps_tracker_.expected_rate_hz   = params_.gps_expected_rate;
        odom_tracker_.sensor_name       = "odometry";
        odom_tracker_.expected_rate_hz  = params_.odometry_expected_rate;

        // Create ROS interfaces with standardized QoS
        scan_sub_ = create_subscription<sensor_msgs::msg::LaserScan>("scan", QosConfig::sensor_qos(),
                                                                     std::bind(&SensorMonitorNode::on_scan, this, _1));
        imu_sub_  = create_subscription<sensor_msgs::msg::Imu>("imu", QosConfig::sensor_qos(),
                                                               std::bind(&SensorMonitorNode::on_imu, this, _1));
        gps_sub_  = create_subscription<sensor_msgs::msg::NavSatFix>("gps", QosConfig::sensor_qos(),
                                                                     std::bind(&SensorMonitorNode::on_gps, this, _1));
        odom_sub_ = create_subscription<nav_msgs::msg::Odometry>("odom", QosConfig::sensor_qos(),
                                                                 std::bind(&SensorMonitorNode::on_odom, this, _1));

        health_pub_ =
            create_publisher<safety_core_msgs::msg::SensorHealth>("safety/sensor_health", QosConfig::state_qos());

        // Create timer for periodic publishing
        timer_ = create_wall_timer(std::chrono::duration<double>(params_.publish_period_s),
                                   std::bind(&SensorMonitorNode::timer_tick, this));

        // Record startup time for grace period
        startup_time_ = get_clock()->now();

        RCLCPP_INFO(get_logger(),
                    "sensor_monitor_node initialized | publish_period=%.3fs healthy_threshold=%.2f "
                    "degraded_threshold=%.2f startup_grace=2.0s",
                    params_.publish_period_s, params_.healthy_threshold, params_.degraded_threshold);
    }

    std::uint64_t SensorMonitorNode::now_ns() const noexcept
    {
        return TimeUtils::now_nanoseconds(get_clock());
    }

    void SensorMonitorNode::record_message(SensorTracker& tracker, std::uint64_t now_ns)
    {
        tracker.last_msg_time_ns.store(now_ns, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(tracker.mutex);
        tracker.msg_times.push_back(now_ns);
    }

    void SensorMonitorNode::prune_window(SensorTracker& tracker, std::uint64_t now_ns, double window_s)
    {
        const std::uint64_t window_ns = TimeUtils::seconds_to_nanoseconds(window_s);
        const std::uint64_t cutoff_ns = (now_ns >= window_ns) ? (now_ns - window_ns) : 0ULL;

        std::lock_guard<std::mutex> lock(tracker.mutex);
        while (!tracker.msg_times.empty() && tracker.msg_times.front() < cutoff_ns)
        {
            tracker.msg_times.pop_front();
        }
    }

    void SensorMonitorNode::on_scan(const sensor_msgs::msg::LaserScan::ConstSharedPtr msg)
    {
        (void)msg;
        record_message(lidar_tracker_, now_ns());
    }

    void SensorMonitorNode::on_imu(const sensor_msgs::msg::Imu::ConstSharedPtr msg)
    {
        (void)msg;
        record_message(imu_tracker_, now_ns());
    }

    void SensorMonitorNode::on_gps(const sensor_msgs::msg::NavSatFix::ConstSharedPtr msg)
    {
        (void)msg;
        record_message(gps_tracker_, now_ns());
    }

    void SensorMonitorNode::on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg)
    {
        (void)msg;
        record_message(odom_tracker_, now_ns());
    }

    safety_core_msgs::msg::SensorHealth SensorMonitorNode::build_sensor_msg(SensorTracker& tracker,
                                                                            std::uint64_t now_ns)
    {
        safety_core_msgs::msg::SensorHealth msg;
        msg.header.stamp     = get_clock()->now();
        msg.header.frame_id  = "sensor_monitor";
        msg.sensor_name      = tracker.sensor_name;
        msg.expected_rate_hz = tracker.expected_rate_hz;

        const std::uint64_t last_time = tracker.last_msg_time_ns.load(std::memory_order_relaxed);
        if (last_time == 0ULL)
        {
            // No messages received yet
            msg.status         = safety_core_msgs::msg::SensorHealth::UNKNOWN;
            msg.update_rate_hz = 0.0;
            msg.data_age_s     = 0.0;
            msg.error_rate     = 1.0;
            msg.detail         = "No messages received";
            return msg;
        }

        // Calculate data age
        const std::uint64_t age_ns = TimeUtils::calculate_age_ns(last_time, now_ns);
        msg.data_age_s             = TimeUtils::nanoseconds_to_seconds(age_ns);

        // Calculate update rate from message count in sliding window
        double update_rate_hz = 0.0;
        {
            std::lock_guard<std::mutex> lock(tracker.mutex);
            if (!tracker.msg_times.empty() && params_.publish_period_s > 0.0)
            {
                update_rate_hz = static_cast<double>(tracker.msg_times.size()) / params_.publish_period_s;
            }
        }
        msg.update_rate_hz = update_rate_hz;

        // Calculate error rate (percentage of dropped messages based on expected rate)
        const double expected_msgs = tracker.expected_rate_hz * params_.publish_period_s;
        const double actual_msgs   = update_rate_hz * params_.publish_period_s;
        double error_rate          = 0.0;
        if (expected_msgs > 0.0)
        {
            error_rate = std::max(0.0, (expected_msgs - actual_msgs) / expected_msgs);
        }
        msg.error_rate = std::min(error_rate, 1.0);

        // Determine status based on rate and data age thresholds
        const double rate_ratio = (tracker.expected_rate_hz > 0.0) ? (update_rate_hz / tracker.expected_rate_hz) : 0.0;
        const double expected_period_s     = (tracker.expected_rate_hz > 0.0) ? (1.0 / tracker.expected_rate_hz) : 0.0;
        const double age_threshold_healthy = 2.0 * expected_period_s;
        const double age_threshold_fault   = 5.0 * expected_period_s;

        if (rate_ratio >= params_.healthy_threshold && msg.data_age_s < age_threshold_healthy)
        {
            msg.status = safety_core_msgs::msg::SensorHealth::HEALTHY;
            msg.detail = "Sensor operating normally";
        }
        else if (rate_ratio >= params_.degraded_threshold && msg.data_age_s < age_threshold_fault)
        {
            msg.status = safety_core_msgs::msg::SensorHealth::DEGRADED;
            msg.detail = "Sensor performance degraded";
        }
        else
        {
            msg.status = safety_core_msgs::msg::SensorHealth::FAULT;
            if (rate_ratio < params_.degraded_threshold)
            {
                msg.detail = "Update rate below threshold";
            }
            else
            {
                msg.detail = "Data age exceeds threshold";
            }
        }

        return msg;
    }

    void SensorMonitorNode::publish_sensor_health(SensorTracker& tracker, std::uint64_t now_ns)
    {
        health_pub_->publish(build_sensor_msg(tracker, now_ns));
    }

    void SensorMonitorNode::timer_tick()
    {
        // Skip publishing during startup grace period to avoid false fault reports
        const double elapsed_s = (get_clock()->now() - startup_time_).seconds();
        if (elapsed_s < kStartupGracePeriodS)
        {
            return;
        }

        const std::uint64_t current_time_ns = now_ns();

        // Prune old message times for each tracker
        prune_window(lidar_tracker_, current_time_ns, params_.publish_period_s);
        prune_window(imu_tracker_, current_time_ns, params_.publish_period_s);
        prune_window(gps_tracker_, current_time_ns, params_.publish_period_s);
        prune_window(odom_tracker_, current_time_ns, params_.publish_period_s);

        // Publish individual sensor health
        publish_sensor_health(lidar_tracker_, current_time_ns);
        publish_sensor_health(imu_tracker_, current_time_ns);
        publish_sensor_health(gps_tracker_, current_time_ns);
        publish_sensor_health(odom_tracker_, current_time_ns);

        // Publish "all" summary with worst-case status
        safety_core_msgs::msg::SensorHealth summary;
        summary.header.stamp     = get_clock()->now();
        summary.header.frame_id  = "sensor_monitor";
        summary.sensor_name      = "all";
        summary.expected_rate_hz = 0.0; // N/A for summary

        uint8_t worst_status = safety_core_msgs::msg::SensorHealth::UNKNOWN;
        std::string detail;

        auto evaluate_tracker = [&](SensorTracker& tracker)
        {
            auto sensor_msg = build_sensor_msg(tracker, current_time_ns);
            if (sensor_msg.status > worst_status)
            {
                worst_status = sensor_msg.status;
                detail       = tracker.sensor_name + ": " + sensor_msg.detail;
            }
        };

        evaluate_tracker(lidar_tracker_);
        evaluate_tracker(imu_tracker_);
        evaluate_tracker(gps_tracker_);
        evaluate_tracker(odom_tracker_);

        summary.status         = worst_status;
        summary.update_rate_hz = 0.0;
        summary.data_age_s     = 0.0;
        summary.error_rate     = 0.0;
        summary.detail         = detail.empty() ? "All sensors unknown" : detail;

        health_pub_->publish(std::move(summary));
    }

} // namespace safety_core_ros
