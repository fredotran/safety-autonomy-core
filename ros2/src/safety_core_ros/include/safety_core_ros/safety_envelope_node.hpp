// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include "safety_core/config/system_config.hpp"
#include "safety_core/safety/safety_envelope.hpp"
#include "safety_core_ros/math_utils.hpp"
#include "safety_core_ros/param_loader.hpp"
#include "safety_core_ros/qos_config.hpp"

#include <atomic>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <safety_core_msgs/msg/envelope_status.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <visualization_msgs/msg/marker_array.hpp>

namespace safety_core_ros
{

    /**
     * @brief Safety envelope evaluation node
     *
     * Consumes a 2D LiDAR scan and odometry, evaluates the safety envelope using
     * safety_core::safety::evaluate_stop_distance() with the configured robot
     * footprint, and publishes:
     *   - /safety/envelope_status (safety_core_msgs/EnvelopeStatus)
     *   - /safety/zone_markers    (visualization_msgs/MarkerArray) for RViz
     *
     * The node processes LiDAR scans to find the nearest obstacle within a
     * forward-facing corridor, then evaluates the safety envelope to determine
     * the appropriate speed limit and safety zone.
     */
    class SafetyEnvelopeNode : public rclcpp::Node
    {
      public:
        explicit SafetyEnvelopeNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions{});

      private:
        // Parameter handling
        void declare_params();
        void load_config_from_params();

        // Callbacks
        void on_scan(const sensor_msgs::msg::LaserScan::ConstSharedPtr msg);
        void on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg);

        // Core functionality
        [[nodiscard]] double
        nearest_obstacle_in_footprint_corridor(const sensor_msgs::msg::LaserScan& scan) const noexcept;

        bool is_scan_point_valid(float range, double min_range, double max_range) const noexcept;
        double calculate_point_distance(float range, double angle) const noexcept;
        bool is_point_in_forward_corridor(double x, double y, double corridor_sq) const noexcept;

        // Publishing
        void publish_envelope(double distance_m, double speed_mps, const std_msgs::msg::Header& header);
        void publish_zone_markers(const safety_core::safety::EnvelopeEvaluation& eval,
                                  const std_msgs::msg::Header& header);

        // Marker creation helpers
        visualization_msgs::msg::Marker create_zone_marker(int id, double radius, safety_core::safety::SafetyZone zone,
                                                           const std_msgs::msg::Header& header) const;
        visualization_msgs::msg::Marker create_status_marker(safety_core::safety::SafetyZone zone,
                                                             const std_msgs::msg::Header& header) const;
        void get_zone_color(safety_core::safety::SafetyZone zone, float& r, float& g, float& b, float& a) const;

        // Configuration
        safety_core::config::SystemConfig config_{};
        safety_core::safety::RobotFootprint footprint_{};

        // Operational parameters
        struct Params
        {
            double corridor_half_width_m  {0.5};
            double scan_min_valid_range_m {0.05};
            double scan_ignore_min_range_m{0.5};  // Ignore obstacles closer than this (workers on robot)
            double startup_grace_period_s {2.0};  // Grace period to allow sensor stabilization
            std::string base_frame         {"base_link"};
        } params_;

        // State
        std::atomic<double> latest_speed_mps_{0.0};
        rclcpp::Time startup_time_;

        // ROS interfaces
        rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;
        rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
        rclcpp::Publisher<safety_core_msgs::msg::EnvelopeStatus>::SharedPtr envelope_pub_;
        rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr marker_pub_;
    };

} // namespace safety_core_ros
