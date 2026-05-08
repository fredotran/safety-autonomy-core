#include "safety_core_ros/safety_drive_bridge_node.hpp"

#include <rclcpp/rclcpp.hpp>

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<safety_core_ros::SafetyDriveBridgeNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
