// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/slip_detector_node.hpp"

#include <rclcpp/rclcpp.hpp>

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<safety_core_ros::SlipDetectorNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
