// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/gps_covariance_adapter.hpp"

#include <algorithm>

using std::placeholders::_1;

namespace safety_core_ros
{

    GpsCovarianceAdapterNode::GpsCovarianceAdapterNode(const rclcpp::NodeOptions& options)
        : rclcpp::Node("gps_covariance_adapter_node", options)
    {
        // Load parameters
        params_.multiplier_excellent         = ParamLoader::load_double(this, "multiplier_excellent", 1.0);
        params_.multiplier_good              = ParamLoader::load_double(this, "multiplier_good", 2.0);
        params_.multiplier_fair              = ParamLoader::load_double(this, "multiplier_fair", 5.0);
        params_.multiplier_poor              = ParamLoader::load_double(this, "multiplier_poor", 10.0);
        params_.min_satellites_for_excellent = ParamLoader::load_int(this, "min_satellites_for_excellent", 8);
        params_.min_satellites_for_good      = ParamLoader::load_int(this, "min_satellites_for_good", 6);
        params_.min_satellites_for_fair      = ParamLoader::load_int(this, "min_satellites_for_fair", 4);
        params_.output_topic                 = ParamLoader::load_string(this, "output_topic", "/gps_adapted");

        // Create ROS interfaces with sensor QoS (best-effort, low latency)
        gps_sub_ = create_subscription<sensor_msgs::msg::NavSatFix>(
            "gps", QosConfig::sensor_qos(), std::bind(&GpsCovarianceAdapterNode::on_gps, this, _1));
        gps_pub_ = create_publisher<sensor_msgs::msg::NavSatFix>(params_.output_topic, QosConfig::sensor_qos());

        RCLCPP_INFO(get_logger(),
                    "gps_covariance_adapter_node initialized | output_topic=%s | "
                    "multipliers=[excellent=%.1f, good=%.1f, fair=%.1f, poor=%.1f]",
                    params_.output_topic.c_str(), params_.multiplier_excellent, params_.multiplier_good,
                    params_.multiplier_fair, params_.multiplier_poor);
    }

    void GpsCovarianceAdapterNode::on_gps(const sensor_msgs::msg::NavSatFix::ConstSharedPtr msg)
    {
        // Start with a copy so all non-covariance fields pass through unchanged
        sensor_msgs::msg::NavSatFix adapted = *msg;

        // Determine quality multiplier based on fix status
        const double multiplier = get_multiplier_for_status(msg->status.status);

        // If covariance is unknown, apply a reasonable base covariance before scaling
        if (msg->position_covariance_type == sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_UNKNOWN)
        {
            // Zero out all elements first to clear any uninitialized/garbage values
            std::fill(adapted.position_covariance.begin(), adapted.position_covariance.end(), 0.0);

            // Base covariance: ~5m horizontal stddev, ~10m vertical stddev
            adapted.position_covariance[0] = 25.0;  // x variance (m^2)
            adapted.position_covariance[4] = 25.0;  // y variance (m^2)
            adapted.position_covariance[8] = 100.0; // z variance (m^2)
        }

        // Scale the diagonal covariance elements by the quality multiplier
        adapted.position_covariance[0] *= multiplier;
        adapted.position_covariance[4] *= multiplier;
        adapted.position_covariance[8] *= multiplier;

        // Mark covariance as known after scaling
        adapted.position_covariance_type = sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_KNOWN;

        gps_pub_->publish(std::move(adapted));
    }

    double GpsCovarianceAdapterNode::get_multiplier_for_status(int8_t status) const
    {
        switch (status)
        {
        case sensor_msgs::msg::NavSatStatus::STATUS_GBAS_FIX:
            return params_.multiplier_excellent;
        case sensor_msgs::msg::NavSatStatus::STATUS_SBAS_FIX:
            return params_.multiplier_good;
        case sensor_msgs::msg::NavSatStatus::STATUS_FIX:
            return params_.multiplier_fair;
        case sensor_msgs::msg::NavSatStatus::STATUS_NO_FIX:
            return params_.multiplier_poor;
        default:
            // Unrecognized status: treat as poor for safety
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
                                 "Unrecognized GPS status: %d. Using poor multiplier.", status);
            return params_.multiplier_poor;
        }
    }

} // namespace safety_core_ros
