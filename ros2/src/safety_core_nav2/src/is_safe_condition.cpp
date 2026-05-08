#include "safety_core_nav2/is_safe_condition.hpp"

#include <behaviortree_cpp_v3/bt_factory.h>
#include <utility>

namespace safety_core_nav2
{

    IsSafeCondition::IsSafeCondition(const std::string& condition_name, const BT::NodeConfiguration& conf)
        : BT::ConditionNode(condition_name, conf)
    {
        node_  = config().blackboard->get<rclcpp::Node::SharedPtr>("node");
        topic_ = "safety/state";
        getInput<std::string>("safety_state_topic", topic_);

        rclcpp::QoS qos(10);
        qos.transient_local();
        sub_ = node_->create_subscription<safety_core_msgs::msg::SafetyState>(
            topic_, qos,
            [this](const safety_core_msgs::msg::SafetyState::ConstSharedPtr msg) { this->on_safety_state(msg); });
    }

    void IsSafeCondition::on_safety_state(const safety_core_msgs::msg::SafetyState::ConstSharedPtr msg)
    {
        last_msg_ = *msg;
        have_msg_ = true;
    }

    BT::NodeStatus IsSafeCondition::tick()
    {
        if (!have_msg_)
        {
            // Conservative default: report failure until we have heard from the
            // supervisor at least once. Nav2 should not start moving without a
            // baseline safety report.
            return BT::NodeStatus::FAILURE;
        }

        if (last_msg_.fault_latched || last_msg_.safe_stop_requested)
        {
            return BT::NodeStatus::FAILURE;
        }

        if (last_msg_.mode == safety_core_msgs::msg::SafetyState::SAFE_STOP ||
            last_msg_.mode == safety_core_msgs::msg::SafetyState::DEGRADED ||
            last_msg_.mode == safety_core_msgs::msg::SafetyState::LOCALIZATION_LOST)
        {
            return BT::NodeStatus::FAILURE;
        }

        bool allow_warning    = true;
        bool allow_protective = false;
        getInput<bool>("allow_warning", allow_warning);
        getInput<bool>("allow_protective", allow_protective);

        using safety_core_msgs::msg::SafetyZone;
        switch (last_msg_.zone.zone)
        {
        case SafetyZone::CLEAR:
            return BT::NodeStatus::SUCCESS;
        case SafetyZone::WARNING:
            return allow_warning ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
        case SafetyZone::PROTECTIVE:
            return allow_protective ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
        case SafetyZone::EMERGENCY:
        default:
            return BT::NodeStatus::FAILURE;
        }
    }

} // namespace safety_core_nav2

// Register plugin entry point so nav2_behavior_tree can load it dynamically.
#include "behaviortree_cpp_v3/bt_factory.h"
BT_REGISTER_NODES(factory)
{
    factory.registerNodeType<safety_core_nav2::IsSafeCondition>("IsSafe");
}
