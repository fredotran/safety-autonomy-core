#pragma once

#include "safety_core/config/system_config.hpp"
#include "safety_core/safety/safety_envelope.hpp"

#include <atomic>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <safety_core_msgs/msg/envelope_status.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <visualization_msgs/msg/marker_array.hpp>

namespace safety_core_ros
{

    // Consumes a 2D LiDAR scan and odometry, evaluates the safety envelope using
    // safety_core::safety::evaluate_stop_distance() with the configured robot
    // footprint, and publishes:
    //   - /safety/envelope_status (safety_core_msgs/EnvelopeStatus)
    //   - /safety/zone_markers    (visualization_msgs/MarkerArray) for RViz
    class SafetyEnvelopeNode : public rclcpp::Node
    {
      public:
        explicit SafetyEnvelopeNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions{});

      private:
        void declare_params();
        void load_config_from_params();

        void on_scan(const sensor_msgs::msg::LaserScan::ConstSharedPtr msg);
        void on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg);

        [[nodiscard]] double
        nearest_obstacle_in_footprint_corridor(const sensor_msgs::msg::LaserScan& scan) const noexcept;

        void publish_envelope(double distance_m, double speed_mps, const std_msgs::msg::Header& header);
        void publish_zone_markers(const safety_core::safety::EnvelopeEvaluation& eval,
                                  const std_msgs::msg::Header& header);

        // Safety core configuration
        safety_core::config::SystemConfig config_{};
        safety_core::safety::RobotFootprint footprint_{};

        // Operational params
        double corridor_half_width_m_{0.5};
        double scan_min_valid_range_m_{0.05};
        std::string base_frame_{"base_link"};

        // Latest measured speed (forward velocity from odometry)
        std::atomic<double> latest_speed_mps_{0.0};

        rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;
        rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
        rclcpp::Publisher<safety_core_msgs::msg::EnvelopeStatus>::SharedPtr envelope_pub_;
        rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr marker_pub_;
    };

} // namespace safety_core_ros
