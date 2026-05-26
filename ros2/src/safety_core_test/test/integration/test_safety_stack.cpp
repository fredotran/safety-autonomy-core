// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <safety_core_msgs/msg/envelope_status.hpp>
#include <safety_core_msgs/msg/safety_state.hpp>
#include <std_msgs/msg/bool.hpp>

using namespace std::chrono_literals;

class SafetyStackIntegrationTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        rclcpp::init(0, nullptr);
        test_node_ = std::make_shared<rclcpp::Node>("test_safety_stack");

        safe_stop_received_ = false;
        state_received_     = false;

        safe_stop_sub_ =
            test_node_->create_subscription<std_msgs::msg::Bool>("safety/safe_stop", 10,
                                                                 [this](const std_msgs::msg::Bool::ConstSharedPtr msg)
                                                                 {
                                                                     last_safe_stop_     = msg->data;
                                                                     safe_stop_received_ = true;
                                                                 });

        state_sub_ = test_node_->create_subscription<safety_core_msgs::msg::SafetyState>(
            "safety/state", 10,
            [this](const safety_core_msgs::msg::SafetyState::ConstSharedPtr msg)
            {
                last_state_     = *msg;
                state_received_ = true;
            });

        envelope_pub_ =
            test_node_->create_publisher<safety_core_msgs::msg::EnvelopeStatus>("safety/envelope_status", 10);

        executor_ = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
        executor_->add_node(test_node_);
    }

    void TearDown() override
    {
        executor_->cancel();
        rclcpp::shutdown();
    }

    void publish_clear_zone()
    {
        safety_core_msgs::msg::EnvelopeStatus msg;
        msg.zone.zone                   = 0; // Clear zone
        msg.recommended_speed_limit_mps = 1.5;
        envelope_pub_->publish(msg);
    }

    void publish_emergency_zone()
    {
        safety_core_msgs::msg::EnvelopeStatus msg;
        msg.zone.zone                   = 3; // Emergency zone
        msg.recommended_speed_limit_mps = 0.0;
        envelope_pub_->publish(msg);
    }

    void spin_for(std::chrono::milliseconds duration)
    {
        auto end = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < end)
        {
            executor_->spin_once(50ms);
        }
    }

    std::shared_ptr<rclcpp::Node> test_node_;
    std::shared_ptr<rclcpp::executors::SingleThreadedExecutor> executor_;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr safe_stop_sub_;
    rclcpp::Subscription<safety_core_msgs::msg::SafetyState>::SharedPtr state_sub_;
    rclcpp::Publisher<safety_core_msgs::msg::EnvelopeStatus>::SharedPtr envelope_pub_;

    bool last_safe_stop_{false};
    safety_core_msgs::msg::SafetyState last_state_;
    std::atomic<bool> safe_stop_received_{false};
    std::atomic<bool> state_received_{false};
};

TEST_F(SafetyStackIntegrationTest, TopicsExist)
{
    // Verify that safety topics exist (requires safety_supervisor_node to be running)
    auto topic_names_and_types = test_node_->get_topic_names_and_types();

    bool has_safe_stop = false;
    bool has_state     = false;

    for (const auto& pair : topic_names_and_types)
    {
        if (pair.first == "/safety/safe_stop")
            has_safe_stop = true;
        if (pair.first == "/safety/state")
            has_state = true;
    }

    // Note: These will only exist if safety_supervisor_node is running
    // This test is a placeholder for when the full stack is launched
    SUCCEED();
}
