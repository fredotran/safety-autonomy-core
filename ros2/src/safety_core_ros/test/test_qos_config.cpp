// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/qos_config.hpp"

#include <gtest/gtest.h>

using safety_core_ros::QosConfig;

TEST(QosConfig, SensorQosIsBestEffortVolatile)
{
    const auto qos = QosConfig::sensor_qos();
    EXPECT_EQ(qos.reliability(), rclcpp::ReliabilityPolicy::BestEffort);
    EXPECT_EQ(qos.durability(), rclcpp::DurabilityPolicy::Volatile);
    EXPECT_EQ(qos.depth(), 5U);
}

TEST(QosConfig, SensorQosDepthIsConfigurable)
{
    EXPECT_EQ(QosConfig::sensor_qos(1).depth(), 1U);
    EXPECT_EQ(QosConfig::sensor_qos(50).depth(), 50U);
}

TEST(QosConfig, ReliableQosIsReliableVolatile)
{
    const auto qos = QosConfig::reliable_qos();
    EXPECT_EQ(qos.reliability(), rclcpp::ReliabilityPolicy::Reliable);
    EXPECT_EQ(qos.durability(), rclcpp::DurabilityPolicy::Volatile);
    EXPECT_EQ(qos.depth(), 10U);
}

TEST(QosConfig, StateAndActuatorMirrorReliableProfile)
{
    // State + actuator traffic must not be lost silently; if this contract
    // changes, downstream consumers relying on guaranteed delivery break.
    for (const auto qos : {QosConfig::state_qos(), QosConfig::actuator_qos()})
    {
        EXPECT_EQ(qos.reliability(), rclcpp::ReliabilityPolicy::Reliable);
        EXPECT_EQ(qos.durability(), rclcpp::DurabilityPolicy::Volatile);
    }
}
