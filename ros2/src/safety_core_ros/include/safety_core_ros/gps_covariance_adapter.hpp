// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include "safety_core_ros/param_loader.hpp"
#include "safety_core_ros/qos_config.hpp"

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <string>

namespace safety_core_ros
{

    /**
     * @brief GPS covariance adapter node for quality-based covariance scaling
     *
     * Subscribes to raw GPS fixes and republishes them with scaled covariance
     * based on fix quality. When the incoming covariance is unknown, a base
     * covariance is applied before scaling. All non-covariance fields pass
     * through unchanged.
     */
    class GpsCovarianceAdapterNode : public rclcpp::Node
    {
      public:
        explicit GpsCovarianceAdapterNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions{});

      private:
        void on_gps(const sensor_msgs::msg::NavSatFix::ConstSharedPtr msg);

        [[nodiscard]] double get_multiplier_for_status(int8_t status) const;

        struct Params
        {
            double multiplier_excellent{1.0};
            double multiplier_good{2.0};
            double multiplier_fair{5.0};
            double multiplier_poor{10.0};
            int min_satellites_for_excellent{8};
            int min_satellites_for_good{6};
            int min_satellites_for_fair{4};
            std::string output_topic{"/gps_adapted"};
        } params_;

        // ROS interfaces
        rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gps_sub_;
        rclcpp::Publisher<sensor_msgs::msg::NavSatFix>::SharedPtr gps_pub_;
    };

} // namespace safety_core_ros
