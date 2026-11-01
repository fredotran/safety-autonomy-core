// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

// Tests for the IsSafe Nav2 BT condition. The condition gates Nav2 motion on
// /safety/state: it must FAIL until a message arrives, must FAIL on faults,
// latched faults, SafeStop/Degraded/LocalizationLost modes and the Emergency
// zone, and must respect the allow_warning / allow_protective ports.

#include "safety_core_nav2/is_safe_condition.hpp"

#include <behaviortree_cpp_v3/bt_factory.h>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <rclcpp/rclcpp.hpp>

using namespace std::chrono_literals;

namespace
{

    class IsSafeConditionTest : public ::testing::Test
    {
      protected:
        static void SetUpTestSuite()
        {
            if (!rclcpp::ok())
            {
                rclcpp::init(0, nullptr);
            }
        }

        static void TearDownTestSuite()
        {
            if (rclcpp::ok())
            {
                rclcpp::shutdown();
            }
        }

        void SetUp() override
        {
            node_ = std::make_shared<rclcpp::Node>("is_safe_test_helper");
            exec_.add_node(node_);

            // transient_local publisher matches the condition's transient_local sub
            pub_ = node_->create_publisher<safety_core_msgs::msg::SafetyState>(
                "safety/state", rclcpp::QoS(rclcpp::KeepLast(10)).transient_local());
        }

        std::unique_ptr<safety_core_nav2::IsSafeCondition> make_condition(const BT::PortsRemapping& ports = {})
        {
            auto blackboard = BT::Blackboard::create();
            blackboard->set("node", node_);

            BT::NodeConfiguration config;
            config.blackboard  = blackboard;
            config.input_ports = ports;
            return std::make_unique<safety_core_nav2::IsSafeCondition>("IsSafe", config);
        }

        void publish_state(std::uint8_t mode, std::uint8_t zone, bool fault_latched = false, bool safe_stop = false)
        {
            safety_core_msgs::msg::SafetyState msg;
            msg.mode                = mode;
            msg.zone.zone           = zone;
            msg.fault_latched       = fault_latched;
            msg.safe_stop_requested = safe_stop;
            pub_->publish(msg);
        }

        // Spin until the next tick reflects the published state. The condition's
        // subscription needs one executor turn to deliver the message.
        BT::NodeStatus tick_after_delivery(safety_core_nav2::IsSafeCondition& cond)
        {
            exec_.spin_some(50ms);
            exec_.spin_some(50ms);
            return cond.tick();
        }

        rclcpp::executors::SingleThreadedExecutor exec_;
        rclcpp::Node::SharedPtr node_;
        rclcpp::Publisher<safety_core_msgs::msg::SafetyState>::SharedPtr pub_;

        using SafetyState = safety_core_msgs::msg::SafetyState;
        using SafetyZone  = safety_core_msgs::msg::SafetyZone;
    };

    TEST_F(IsSafeConditionTest, FailsUntilFirstMessage)
    {
        auto cond = make_condition();
        // Conservative default: no supervisor report -> not safe.
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::FAILURE);
    }

    TEST_F(IsSafeConditionTest, ClearZoneIdleModeSucceeds)
    {
        auto cond = make_condition();
        publish_state(SafetyState::IDLE, SafetyZone::CLEAR);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::SUCCESS);
    }

    TEST_F(IsSafeConditionTest, MovingClearSucceeds)
    {
        auto cond = make_condition();
        publish_state(SafetyState::MOVING, SafetyZone::CLEAR);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::SUCCESS);
    }

    TEST_F(IsSafeConditionTest, WarningZoneAllowedByDefault)
    {
        auto cond = make_condition();
        publish_state(SafetyState::MOVING, SafetyZone::WARNING);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::SUCCESS);
    }

    TEST_F(IsSafeConditionTest, WarningZoneRejectedWhenDisallowed)
    {
        auto cond = make_condition({{"allow_warning", "false"}});
        publish_state(SafetyState::MOVING, SafetyZone::WARNING);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::FAILURE);
    }

    TEST_F(IsSafeConditionTest, ProtectiveZoneRejectedByDefault)
    {
        auto cond = make_condition();
        publish_state(SafetyState::MOVING, SafetyZone::PROTECTIVE);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::FAILURE);
    }

    TEST_F(IsSafeConditionTest, ProtectiveZoneAllowedWhenEnabled)
    {
        auto cond = make_condition({{"allow_protective", "true"}});
        publish_state(SafetyState::MOVING, SafetyZone::PROTECTIVE);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::SUCCESS);
    }

    TEST_F(IsSafeConditionTest, EmergencyZoneAlwaysFails)
    {
        auto cond = make_condition({{"allow_warning", "true"}, {"allow_protective", "true"}});
        publish_state(SafetyState::MOVING, SafetyZone::EMERGENCY);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::FAILURE);
    }

    TEST_F(IsSafeConditionTest, SafeStopModeFails)
    {
        auto cond = make_condition();
        publish_state(SafetyState::SAFE_STOP, SafetyZone::CLEAR);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::FAILURE);
    }

    TEST_F(IsSafeConditionTest, DegradedModeFails)
    {
        auto cond = make_condition();
        publish_state(SafetyState::DEGRADED, SafetyZone::CLEAR);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::FAILURE);
    }

    TEST_F(IsSafeConditionTest, LocalizationLostModeFails)
    {
        auto cond = make_condition();
        publish_state(SafetyState::LOCALIZATION_LOST, SafetyZone::CLEAR);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::FAILURE);
    }

    TEST_F(IsSafeConditionTest, LatchedFaultFailsRegardlessOfZone)
    {
        auto cond = make_condition();
        publish_state(SafetyState::IDLE, SafetyZone::CLEAR, /*fault_latched=*/true);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::FAILURE);
    }

    TEST_F(IsSafeConditionTest, SafeStopRequestedFailsRegardlessOfZone)
    {
        auto cond = make_condition();
        publish_state(SafetyState::IDLE, SafetyZone::CLEAR, false, /*safe_stop=*/true);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::FAILURE);
    }

    TEST_F(IsSafeConditionTest, CustomTopicIsHonored)
    {
        auto custom_pub = node_->create_publisher<safety_core_msgs::msg::SafetyState>(
            "custom/safety", rclcpp::QoS(rclcpp::KeepLast(10)).transient_local());

        auto cond = make_condition({{"safety_state_topic", "custom/safety"}});

        // Publish on the default topic first: condition must remain FAILURE.
        publish_state(SafetyState::IDLE, SafetyZone::CLEAR);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::FAILURE);

        safety_core_msgs::msg::SafetyState msg;
        msg.mode      = SafetyState::IDLE;
        msg.zone.zone = SafetyZone::CLEAR;
        custom_pub->publish(msg);
        EXPECT_EQ(tick_after_delivery(*cond), BT::NodeStatus::SUCCESS);
    }

} // namespace
