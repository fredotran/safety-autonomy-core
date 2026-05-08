// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/safety_envelope_node.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

using safety_core::safety::SafetyZone;
using std::placeholders::_1;

namespace safety_core_ros
{

    SafetyEnvelopeNode::SafetyEnvelopeNode(const rclcpp::NodeOptions& options)
        : rclcpp::Node("safety_envelope_node", options)
    {
        // Load parameters using utility classes
        load_config_from_params();

        // Create ROS interfaces with standardized QoS
        scan_sub_ = create_subscription<sensor_msgs::msg::LaserScan>("scan", QosConfig::sensor_qos(),
                                                                     std::bind(&SafetyEnvelopeNode::on_scan, this, _1));

        odom_sub_ = create_subscription<nav_msgs::msg::Odometry>("odom", QosConfig::sensor_qos(),
                                                                 std::bind(&SafetyEnvelopeNode::on_odom, this, _1));

        envelope_pub_ =
            create_publisher<safety_core_msgs::msg::EnvelopeStatus>("safety/envelope_status", QosConfig::state_qos());

        marker_pub_ =
            create_publisher<visualization_msgs::msg::MarkerArray>("safety/zone_markers", QosConfig::state_qos());

        // Initialize startup time for grace period
        startup_time_ = get_clock()->now();

        RCLCPP_INFO(get_logger(),
                    "safety_envelope_node up | footprint=%.2fx%.2fm overhang=%.2fm | "
                    "max_speed=%.2f decel=%.2f buffer=%.2f",
                    footprint_.length_m, footprint_.width_m, footprint_.front_overhang_m,
                    config_.envelope.max_speed_mps, config_.envelope.max_comfort_decel_mps2,
                    config_.envelope.safety_buffer_m);
    }

    void SafetyEnvelopeNode::declare_params()
    {
        // Parameters are declared via YAML file, no need to declare here
    }

    void SafetyEnvelopeNode::load_config_from_params()
    {
        // Load envelope configuration
        config_.envelope.max_speed_mps  = ParamLoader::load_double(this, "envelope.max_speed_mps", 1.5);
        config_.envelope.max_accel_mps2 = ParamLoader::load_double(this, "envelope.max_accel_mps2", 0.75);
        config_.envelope.max_comfort_decel_mps2 =
            ParamLoader::load_double(this, "envelope.max_comfort_decel_mps2", 1.0);
        config_.envelope.control_latency_s = ParamLoader::load_double(this, "envelope.control_latency_s", 0.04);
        config_.envelope.safety_buffer_m   = ParamLoader::load_double(this, "envelope.safety_buffer_m", 0.3);

        // Load footprint configuration
        footprint_.length_m         = ParamLoader::load_double(this, "footprint.length_m", 0.8);
        footprint_.width_m          = ParamLoader::load_double(this, "footprint.width_m", 0.6);
        footprint_.front_overhang_m = ParamLoader::load_double(this, "footprint.front_overhang_m", 0.1);

        // Load operational parameters
        params_.corridor_half_width_m   = ParamLoader::load_double(this, "corridor_half_width_m", 0.5);
        params_.scan_min_valid_range_m  = ParamLoader::load_double(this, "scan_min_valid_range_m", 0.05);
        params_.scan_ignore_min_range_m = ParamLoader::load_double(this, "scan_ignore_min_range_m", 0.5);
        params_.startup_grace_period_s  = ParamLoader::load_double(this, "startup_grace_period_s", 2.0);
        params_.base_frame              = ParamLoader::load_string(this, "base_frame", "base_link");
    }

    void SafetyEnvelopeNode::on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg)
    {
        // Store forward velocity (magnitude_safe = max(0, vx))
        latest_speed_mps_.store(msg->twist.twist.linear.x, std::memory_order_relaxed);
    }

    void SafetyEnvelopeNode::on_scan(const sensor_msgs::msg::LaserScan::ConstSharedPtr msg)
    {
        // Find nearest obstacle and evaluate envelope
        const double distance_m = nearest_obstacle_in_footprint_corridor(*msg);
        const double speed_mps  = MathUtils::max(0.0, latest_speed_mps_.load(std::memory_order_relaxed));
        publish_envelope(distance_m, speed_mps, msg->header);
    }

    double
    SafetyEnvelopeNode::nearest_obstacle_in_footprint_corridor(const sensor_msgs::msg::LaserScan& scan) const noexcept
    {
        // Only consider returns inside a forward corridor of width
        // (footprint.width + 2 * corridor_half_width) so that returns left/right
        // of the AGV body don't trigger zone changes when nothing is actually
        // in the path. Coordinates are in the scan's frame (typically lidar_link
        // co-located with base_link forward axis).

        // Pre-compute corridor parameters
        const double corridor    = (footprint_.width_m * 0.5) + params_.corridor_half_width_m;
        const double corridor_sq = corridor * corridor; // Squared for fast comparison

        double min_d = std::numeric_limits<double>::infinity();

        // Process each scan point
        const std::vector<float>& ranges = scan.ranges;
        const std::size_t count          = ranges.size();

        for (std::size_t i = 0U; i < count; ++i)
        {
            const float r = ranges[i];

            // Fast rejection: check range validity
            if (!is_scan_point_valid(r, params_.scan_min_valid_range_m, scan.range_max))
            {
                continue;
            }

            // Calculate point position
            const double angle = scan.angle_min + static_cast<double>(i) * scan.angle_increment;
            const double x     = calculate_point_distance(r, angle);
            const double y     = static_cast<double>(r) * std::sin(angle);

            // Check if point is in forward corridor
            if (!is_point_in_forward_corridor(x, y, corridor_sq))
            {
                continue;
            }

            // Ignore obstacles closer than scan_ignore_min_range_m (workers on robot)
            if (x < params_.scan_ignore_min_range_m)
            {
                continue;
            }

            // Update minimum distance
            min_d = MathUtils::min(x, min_d);
        }
        return min_d;
    }

    bool SafetyEnvelopeNode::is_scan_point_valid(float range, double min_range, double max_range) const noexcept
    {
        return MathUtils::is_finite(range) && MathUtils::is_in_range(range, min_range, max_range);
    }

    double SafetyEnvelopeNode::calculate_point_distance(float range, double angle) const noexcept
    {
        return static_cast<double>(range) * std::cos(angle);
    }

    bool SafetyEnvelopeNode::is_point_in_forward_corridor(double x, double y, double corridor_sq) const noexcept
    {
        return x > 0.0 && MathUtils::is_in_corridor_squared(y, corridor_sq);
    }

    void SafetyEnvelopeNode::publish_envelope(double distance_m, double speed_mps, const std_msgs::msg::Header& header)
    {
        // Check if we're still in the startup grace period
        const rclcpp::Time now     = get_clock()->now();
        const double elapsed_s     = (now - startup_time_).seconds();
        const bool in_grace_period = elapsed_s < params_.startup_grace_period_s;

        // Handle invalid scan data (no valid obstacles detected)
        if (!MathUtils::is_finite(distance_m) || distance_m < 0.0)
        {
            // During grace period, assume clear zone to allow startup
            // After grace period, assume warning zone for safety
            safety_core_msgs::msg::EnvelopeStatus status;
            status.header                 = header;
            status.distance_to_obstacle_m = -1.0; // Invalid/unknown
            status.current_speed_mps      = speed_mps;
            status.within_envelope        = in_grace_period;
            status.zone.zone = in_grace_period ? static_cast<std::uint8_t>(safety_core::safety::SafetyZone::Clear)
                                               : static_cast<std::uint8_t>(safety_core::safety::SafetyZone::Warning);
            status.stopping_distance_m         = config_.envelope.safety_buffer_m;
            status.required_clearance_m        = config_.envelope.safety_buffer_m;
            status.recommended_speed_limit_mps = in_grace_period
                                                     ? config_.envelope.max_speed_mps
                                                     : std::min(speed_mps, config_.envelope.max_speed_mps * 0.5);
            status.footprint_length_m          = footprint_.length_m;
            status.footprint_width_m           = footprint_.width_m;
            status.footprint_front_overhang_m  = footprint_.front_overhang_m;

            envelope_pub_->publish(std::move(status));
            return;
        }

        const auto eval =
            safety_core::safety::evaluate_stop_distance(distance_m, speed_mps, config_.envelope, footprint_);

        // During grace period, override emergency zone to warning zone
        safety_core::safety::EnvelopeEvaluation modified_eval = eval;
        if (in_grace_period && eval.zone == safety_core::safety::SafetyZone::Emergency)
        {
            modified_eval.zone                        = safety_core::safety::SafetyZone::Warning;
            modified_eval.recommended_speed_limit_mps = std::min(speed_mps, config_.envelope.max_speed_mps * 0.5);
        }

        // Use move semantics to avoid copies
        safety_core_msgs::msg::EnvelopeStatus status;
        status.header                      = header;
        status.distance_to_obstacle_m      = MathUtils::is_finite(distance_m) ? distance_m : -1.0;
        status.current_speed_mps           = speed_mps;
        status.within_envelope             = modified_eval.within_envelope;
        status.zone.zone                   = static_cast<std::uint8_t>(modified_eval.zone);
        status.stopping_distance_m         = modified_eval.stopping_distance;
        status.required_clearance_m        = modified_eval.required_clearance;
        status.recommended_speed_limit_mps = modified_eval.recommended_speed_limit_mps;
        status.footprint_length_m          = footprint_.length_m;
        status.footprint_width_m           = footprint_.width_m;
        status.footprint_front_overhang_m  = footprint_.front_overhang_m;

        envelope_pub_->publish(std::move(status));

        // Only publish markers if there are subscribers (reduce CPU overhead)
        if (marker_pub_->get_subscription_count() > 0)
        {
            publish_zone_markers(modified_eval, header);
        }
    }

    void SafetyEnvelopeNode::publish_zone_markers(const safety_core::safety::EnvelopeEvaluation& eval,
                                                  const std_msgs::msg::Header& header)
    {
        // Pre-allocate marker array to avoid dynamic allocations
        visualization_msgs::msg::MarkerArray array;
        array.markers.reserve(4); // Pre-allocate for 4 markers

        // Calculate zone radii
        const double emergency_r  = MathUtils::max(0.05, config_.envelope.safety_buffer_m);
        const double protective_r = MathUtils::max(emergency_r + 0.05, eval.required_clearance);
        const double warning_r    = protective_r * 1.5;

        // Add zone markers
        array.markers.push_back(create_zone_marker(0, warning_r, SafetyZone::Warning, header));
        array.markers.push_back(create_zone_marker(1, protective_r, SafetyZone::Protective, header));
        array.markers.push_back(create_zone_marker(2, emergency_r, SafetyZone::Emergency, header));

        // Add status badge marker
        array.markers.push_back(create_status_marker(eval.zone, header));

        marker_pub_->publish(std::move(array));
    }

    visualization_msgs::msg::Marker SafetyEnvelopeNode::create_zone_marker(int id, double radius, SafetyZone zone,
                                                                           const std_msgs::msg::Header& header) const
    {
        visualization_msgs::msg::Marker marker;
        marker.header             = header;
        marker.header.frame_id    = params_.base_frame;
        marker.ns                 = "safety_zones";
        marker.id                 = id;
        marker.type               = visualization_msgs::msg::Marker::CYLINDER;
        marker.action             = visualization_msgs::msg::Marker::ADD;
        marker.pose.position.x    = 0.0;
        marker.pose.position.y    = 0.0;
        marker.pose.position.z    = 0.02;
        marker.pose.orientation.w = 1.0;
        marker.scale.x            = radius * 2.0;
        marker.scale.y            = radius * 2.0;
        marker.scale.z            = 0.02;

        float r = 0.0F, g = 0.0F, b = 0.0F, a = 0.0F;
        get_zone_color(zone, r, g, b, a);
        marker.color.r  = r;
        marker.color.g  = g;
        marker.color.b  = b;
        marker.color.a  = a;
        marker.lifetime = rclcpp::Duration::from_seconds(0.5);

        return marker;
    }

    visualization_msgs::msg::Marker SafetyEnvelopeNode::create_status_marker(SafetyZone zone,
                                                                             const std_msgs::msg::Header& header) const
    {
        visualization_msgs::msg::Marker marker;
        marker.header.frame_id    = params_.base_frame;
        marker.header.stamp       = header.stamp;
        marker.ns                 = "safety_zone_badge";
        marker.id                 = 0;
        marker.type               = visualization_msgs::msg::Marker::SPHERE;
        marker.action             = visualization_msgs::msg::Marker::ADD;
        marker.pose.position.x    = 0.0;
        marker.pose.position.y    = 0.0;
        marker.pose.position.z    = 0.6;
        marker.pose.orientation.w = 1.0;
        marker.scale.x            = 0.18;
        marker.scale.y            = 0.18;
        marker.scale.z            = 0.18;

        float r = 0.0F, g = 0.0F, b = 0.0F, a = 0.0F;
        get_zone_color(zone, r, g, b, a);
        marker.color.r  = r;
        marker.color.g  = g;
        marker.color.b  = b;
        marker.color.a  = 1.0F;
        marker.lifetime = rclcpp::Duration::from_seconds(0.5);

        return marker;
    }

    void SafetyEnvelopeNode::get_zone_color(SafetyZone zone, float& r, float& g, float& b, float& a) const
    {
        a = 0.35F;
        switch (zone)
        {
        case SafetyZone::Clear:
            r = 0.1F;
            g = 0.8F;
            b = 0.2F;
            break;
        case SafetyZone::Warning:
            r = 1.0F;
            g = 0.85F;
            b = 0.0F;
            break;
        case SafetyZone::Protective:
            r = 1.0F;
            g = 0.5F;
            b = 0.0F;
            break;
        case SafetyZone::Emergency:
            r = 0.95F;
            g = 0.05F;
            b = 0.05F;
            a = 0.55F;
            break;
        }
    }

} // namespace safety_core_ros
