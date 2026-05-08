#pragma once

#include <behaviortree_cpp_v3/condition_node.h>
#include <rclcpp/rclcpp.hpp>
#include <safety_core_msgs/msg/safety_state.hpp>
#include <string>

namespace safety_core_nav2
{

    // Behavior tree condition that succeeds when the safety_core supervisor
    // reports a safe operating state. Subscribes to /safety/state via
    // nav2_behavior_tree::BtTopicSubNode infrastructure.
    class IsSafeCondition : public BT::ConditionNode
    {
      public:
        IsSafeCondition(const std::string& condition_name, const BT::NodeConfiguration& conf);

        IsSafeCondition() = delete;

        BT::NodeStatus tick() override;

        static BT::PortsList providedPorts()
        {
            return {
                BT::InputPort<std::string>("safety_state_topic", std::string("safety/state"),
                                           "Topic to subscribe to for safety state."),
                BT::InputPort<bool>("allow_warning", true, "Treat WARNING zone as safe."),
                BT::InputPort<bool>("allow_protective", false, "Treat PROTECTIVE zone as safe."),
            };
        }

      private:
        void on_safety_state(const safety_core_msgs::msg::SafetyState::ConstSharedPtr msg);

        rclcpp::Node::SharedPtr node_;
        rclcpp::Subscription<safety_core_msgs::msg::SafetyState>::SharedPtr sub_;
        std::string topic_;
        safety_core_msgs::msg::SafetyState last_msg_;
        bool have_msg_{false};
    };

} // namespace safety_core_nav2
