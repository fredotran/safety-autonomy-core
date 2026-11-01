// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/ros_clock.hpp"
#include "safety_core_ros/ros_diagnostic_transport.hpp"
#include "safety_core_ros/ros_health_monitor.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <safety_core/state_machine/state_machine.hpp>

namespace
{

    class RosBridgesTest : public ::testing::Test
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
    };

    TEST_F(RosBridgesTest, RosClockConvertsRosTimeToCoreTimePoint)
    {
        auto clock = std::make_shared<rclcpp::Clock>(RCL_SYSTEM_TIME);
        safety_core_ros::RosClock adapter(clock);

        // The adapted value must sit between the ROS clock readings taken
        // immediately before and after the call.
        const auto before = clock->now().nanoseconds();
        const auto tp_ns  = adapter.now().time_since_epoch().count();
        const auto after  = clock->now().nanoseconds();

        EXPECT_GT(tp_ns, 0);
        EXPECT_GE(tp_ns, before);
        EXPECT_LE(tp_ns, after);
    }

    TEST_F(RosBridgesTest, DiagnosticTransportCountsTruncations)
    {
        auto node = std::make_shared<rclcpp::Node>("diag_transport_test");
        auto pub  = node->create_publisher<safety_core_msgs::msg::DiagnosticEvent>("diag_test", 10);
        safety_core_ros::RosDiagnosticTransport transport(pub);

        safety_core::diag::DiagnosticEvent event;
        event.set_topic("test.topic");
        event.set_payload("payload");
        event.timestamp_ns = 1'500'000'000ULL;

        transport.publish(event);
        EXPECT_EQ(transport.topic_truncation_count(), 0U);
        EXPECT_EQ(transport.payload_truncation_count(), 0U);
        EXPECT_EQ(transport.publish_failure_count(), 0U);

        // Topic longer than the 64-byte fixed buffer must set topic_truncated.
        event.set_topic(std::string(100, 't'));
        event.set_payload(std::string(300, 'p'));
        EXPECT_TRUE(event.topic_truncated);
        EXPECT_TRUE(event.payload_truncated);
        transport.publish(event);
        EXPECT_EQ(transport.topic_truncation_count(), 1U);
        EXPECT_EQ(transport.payload_truncation_count(), 1U);
    }

    TEST_F(RosBridgesTest, DiagnosticTransportNullPublisherFailsCounted)
    {
        safety_core_ros::RosDiagnosticTransport transport(nullptr);

        safety_core::diag::DiagnosticEvent event;
        event.set_topic("t");
        transport.publish(event);

        EXPECT_EQ(transport.publish_failure_count(), 1U);
    }

    TEST_F(RosBridgesTest, DiagnosticTransportPublishesConvertedMessage)
    {
        auto node = std::make_shared<rclcpp::Node>("diag_transport_pub");
        auto pub  = node->create_publisher<safety_core_msgs::msg::DiagnosticEvent>("diag_out", 10);

        safety_core_msgs::msg::DiagnosticEvent received;
        bool got = false;
        auto sub = node->create_subscription<safety_core_msgs::msg::DiagnosticEvent>(
            "diag_out", 10,
            [&](const safety_core_msgs::msg::DiagnosticEvent::ConstSharedPtr msg)
            {
                received = *msg;
                got      = true;
            });

        safety_core_ros::RosDiagnosticTransport transport(pub);
        safety_core::diag::DiagnosticEvent event;
        event.set_topic("executor.deadline_miss");
        event.set_payload("task=42");
        event.timestamp_ns = 2'000'000'123ULL;
        transport.publish(event);

        rclcpp::executors::SingleThreadedExecutor exec;
        exec.add_node(node);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!got && std::chrono::steady_clock::now() < deadline)
        {
            exec.spin_some(std::chrono::milliseconds(10));
        }

        ASSERT_TRUE(got) << "no DiagnosticEvent received within 5 s";
        EXPECT_EQ(received.topic, "executor.deadline_miss");
        EXPECT_EQ(received.payload, "task=42");
        EXPECT_EQ(received.source_timestamp_ns, 2'000'000'123ULL);
        EXPECT_EQ(received.header.stamp.sec, 2);
        EXPECT_EQ(received.header.stamp.nanosec, 123U);
        EXPECT_FALSE(received.topic_truncated);
        EXPECT_FALSE(received.payload_truncated);
    }

    TEST_F(RosBridgesTest, HealthMonitorLogsWithoutThrowing)
    {
        safety_core_ros::RosHealthMonitor monitor(rclcpp::get_logger("health_test"));
        // Noexcept contract: callbacks must never throw even for edge inputs.
        monitor.on_mode_transition(safety_core::sm::Mode::Idle, safety_core::sm::Mode::Moving);
        monitor.on_mode_transition(safety_core::sm::Mode::SafeStop, safety_core::sm::Mode::Init);
        monitor.on_fault_latched(0xFFFFU);
        monitor.on_fault_latched(0U);
        SUCCEED();
    }

} // namespace
