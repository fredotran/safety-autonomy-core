#include "safety_core_ros/safety_envelope_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

using safety_core::safety::SafetyZone;
using std::placeholders::_1;

namespace safety_core_ros
{

    SafetyEnvelopeNode::SafetyEnvelopeNode(const rclcpp::NodeOptions& options)
        : rclcpp::Node("safety_envelope_node", options)
    {
        declare_params();
        load_config_from_params();

        rclcpp::QoS sensor_qos = rclcpp::SensorDataQoS();
        rclcpp::QoS reliable_qos(10);

        scan_sub_ = create_subscription<sensor_msgs::msg::LaserScan>("scan", sensor_qos,
                                                                     std::bind(&SafetyEnvelopeNode::on_scan, this, _1));
        odom_sub_ = create_subscription<nav_msgs::msg::Odometry>("odom", sensor_qos,
                                                                 std::bind(&SafetyEnvelopeNode::on_odom, this, _1));

        envelope_pub_ = create_publisher<safety_core_msgs::msg::EnvelopeStatus>("safety/envelope_status", reliable_qos);
        marker_pub_   = create_publisher<visualization_msgs::msg::MarkerArray>("safety/zone_markers", reliable_qos);

        RCLCPP_INFO(get_logger(),
                    "safety_envelope_node up | footprint=%.2fx%.2fm overhang=%.2fm | "
                    "max_speed=%.2f decel=%.2f buffer=%.2f",
                    footprint_.length_m, footprint_.width_m, footprint_.front_overhang_m,
                    config_.envelope.max_speed_mps, config_.envelope.max_comfort_decel_mps2,
                    config_.envelope.safety_buffer_m);
    }

    void SafetyEnvelopeNode::declare_params()
    {
        declare_parameter<double>("envelope.max_speed_mps", 1.5);
        declare_parameter<double>("envelope.max_accel_mps2", 0.75);
        declare_parameter<double>("envelope.max_comfort_decel_mps2", 1.0);
        declare_parameter<double>("envelope.control_latency_s", 0.04);
        declare_parameter<double>("envelope.safety_buffer_m", 0.3);

        declare_parameter<double>("footprint.length_m", 0.8);
        declare_parameter<double>("footprint.width_m", 0.6);
        declare_parameter<double>("footprint.front_overhang_m", 0.1);

        declare_parameter<double>("corridor_half_width_m", 0.5);
        declare_parameter<double>("scan_min_valid_range_m", 0.05);
        declare_parameter<std::string>("base_frame", "base_link");
    }

    void SafetyEnvelopeNode::load_config_from_params()
    {
        config_.envelope.max_speed_mps          = get_parameter("envelope.max_speed_mps").as_double();
        config_.envelope.max_accel_mps2         = get_parameter("envelope.max_accel_mps2").as_double();
        config_.envelope.max_comfort_decel_mps2 = get_parameter("envelope.max_comfort_decel_mps2").as_double();
        config_.envelope.control_latency_s      = get_parameter("envelope.control_latency_s").as_double();
        config_.envelope.safety_buffer_m        = get_parameter("envelope.safety_buffer_m").as_double();

        footprint_.length_m         = get_parameter("footprint.length_m").as_double();
        footprint_.width_m          = get_parameter("footprint.width_m").as_double();
        footprint_.front_overhang_m = get_parameter("footprint.front_overhang_m").as_double();

        corridor_half_width_m_  = get_parameter("corridor_half_width_m").as_double();
        scan_min_valid_range_m_ = get_parameter("scan_min_valid_range_m").as_double();
        base_frame_             = get_parameter("base_frame").as_string();
    }

    void SafetyEnvelopeNode::on_odom(const nav_msgs::msg::Odometry::ConstSharedPtr msg)
    {
        // Forward velocity in body frame is twist.linear.x. magnitude_safe = max(0, vx).
        latest_speed_mps_.store(msg->twist.twist.linear.x, std::memory_order_relaxed);
    }

    void SafetyEnvelopeNode::on_scan(const sensor_msgs::msg::LaserScan::ConstSharedPtr msg)
    {
        const double distance_m = nearest_obstacle_in_footprint_corridor(*msg);
        const double speed_mps  = std::max(0.0, latest_speed_mps_.load(std::memory_order_relaxed));
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
        const double corridor = (footprint_.width_m * 0.5) + corridor_half_width_m_;
        double min_d          = std::numeric_limits<double>::infinity();

        const std::size_t count = scan.ranges.size();
        for (std::size_t i = 0U; i < count; ++i)
        {
            const float r = scan.ranges[i];
            if (!std::isfinite(r) || r < scan_min_valid_range_m_ || r > scan.range_max)
            {
                continue;
            }
            const double angle = scan.angle_min + static_cast<double>(i) * scan.angle_increment;
            const double x     = static_cast<double>(r) * std::cos(angle);
            const double y     = static_cast<double>(r) * std::sin(angle);

            // Forward half-plane only (x > 0). Side returns (x <= 0) are ignored.
            if (x <= 0.0)
            {
                continue;
            }
            if (std::fabs(y) > corridor)
            {
                continue;
            }
            // Use forward distance x as the path-aligned distance for envelope
            // evaluation (conservative; underestimates curved-path clearance).
            if (x < min_d)
            {
                min_d = x;
            }
        }
        return min_d;
    }

    void SafetyEnvelopeNode::publish_envelope(double distance_m, double speed_mps, const std_msgs::msg::Header& header)
    {
        const auto eval =
            safety_core::safety::evaluate_stop_distance(distance_m, speed_mps, config_.envelope, footprint_);

        safety_core_msgs::msg::EnvelopeStatus status;
        status.header                      = header;
        status.distance_to_obstacle_m      = std::isfinite(distance_m) ? distance_m : -1.0;
        status.current_speed_mps           = speed_mps;
        status.within_envelope             = eval.within_envelope;
        status.zone.zone                   = static_cast<std::uint8_t>(eval.zone);
        status.stopping_distance_m         = eval.stopping_distance;
        status.required_clearance_m        = eval.required_clearance;
        status.recommended_speed_limit_mps = eval.recommended_speed_limit_mps;
        status.footprint_length_m          = footprint_.length_m;
        status.footprint_width_m           = footprint_.width_m;
        status.footprint_front_overhang_m  = footprint_.front_overhang_m;

        envelope_pub_->publish(std::move(status));
        publish_zone_markers(eval, header);
    }

    namespace
    {

        void color_for_zone(SafetyZone zone, float& r, float& g, float& b, float& a)
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

    } // namespace

    void SafetyEnvelopeNode::publish_zone_markers(const safety_core::safety::EnvelopeEvaluation& eval,
                                                  const std_msgs::msg::Header& header)
    {
        visualization_msgs::msg::MarkerArray array;

        // Three concentric semicircles in front of the AGV: warning, protective,
        // emergency. Radii derived from envelope evaluation parameters.
        const double emergency_r  = std::max(0.05, config_.envelope.safety_buffer_m);
        const double protective_r = std::max(emergency_r + 0.05, eval.required_clearance);
        const double warning_r    = protective_r * 1.5;

        auto make_disc = [&](int id, double radius, SafetyZone zone)
        {
            visualization_msgs::msg::Marker m;
            m.header             = header;
            m.header.frame_id    = base_frame_;
            m.ns                 = "safety_zones";
            m.id                 = id;
            m.type               = visualization_msgs::msg::Marker::CYLINDER;
            m.action             = visualization_msgs::msg::Marker::ADD;
            m.pose.position.x    = 0.0;
            m.pose.position.y    = 0.0;
            m.pose.position.z    = 0.02;
            m.pose.orientation.w = 1.0;
            m.scale.x            = radius * 2.0;
            m.scale.y            = radius * 2.0;
            m.scale.z            = 0.02;
            float r, g, b, a;
            color_for_zone(zone, r, g, b, a);
            m.color.r  = r;
            m.color.g  = g;
            m.color.b  = b;
            m.color.a  = a;
            m.lifetime = rclcpp::Duration::from_seconds(0.5);
            return m;
        };

        array.markers.push_back(make_disc(0, warning_r, SafetyZone::Warning));
        array.markers.push_back(make_disc(1, protective_r, SafetyZone::Protective));
        array.markers.push_back(make_disc(2, emergency_r, SafetyZone::Emergency));

        // A small marker showing the active zone as a colored sphere above the AGV.
        {
            visualization_msgs::msg::Marker badge;
            badge.header.frame_id    = base_frame_;
            badge.header.stamp       = header.stamp;
            badge.ns                 = "safety_zone_badge";
            badge.id                 = 0;
            badge.type               = visualization_msgs::msg::Marker::SPHERE;
            badge.action             = visualization_msgs::msg::Marker::ADD;
            badge.pose.position.x    = 0.0;
            badge.pose.position.y    = 0.0;
            badge.pose.position.z    = 0.6;
            badge.pose.orientation.w = 1.0;
            badge.scale.x            = 0.18;
            badge.scale.y            = 0.18;
            badge.scale.z            = 0.18;
            float r, g, b, a;
            color_for_zone(eval.zone, r, g, b, a);
            badge.color.r  = r;
            badge.color.g  = g;
            badge.color.b  = b;
            badge.color.a  = 1.0F;
            badge.lifetime = rclcpp::Duration::from_seconds(0.5);
            array.markers.push_back(badge);
        }

        marker_pub_->publish(std::move(array));
    }

} // namespace safety_core_ros
