// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/ros_drive_actuator_bridge.hpp"
#include "safety_core_ros/ros_imu_bridge.hpp"
#include "safety_core_ros/ros_odometry_bridge.hpp"

#include <gtest/gtest.h>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/imu.hpp>

namespace safety_core_ros
{

    // ---- RosImuBridge tests -------------------------------------------------------

    TEST(RosImuBridge, FieldMapping)
    {
        sensor_msgs::msg::Imu msg;
        msg.header.stamp.sec      = 1U;
        msg.header.stamp.nanosec  = 500'000'000U;
        msg.angular_velocity.x    = 0.1;
        msg.angular_velocity.y    = 0.2;
        msg.angular_velocity.z    = 0.3;
        msg.linear_acceleration.x = 1.0;
        msg.linear_acceleration.y = 2.0;
        msg.linear_acceleration.z = 9.81;

        const auto sample = RosImuBridge::to_sample(msg);

        EXPECT_DOUBLE_EQ(sample.angular_velocity_x_radps, 0.1);
        EXPECT_DOUBLE_EQ(sample.angular_velocity_y_radps, 0.2);
        EXPECT_DOUBLE_EQ(sample.angular_velocity_z_radps, 0.3);
        EXPECT_DOUBLE_EQ(sample.linear_accel_x_mps2, 1.0);
        EXPECT_DOUBLE_EQ(sample.linear_accel_y_mps2, 2.0);
        EXPECT_DOUBLE_EQ(sample.linear_accel_z_mps2, 9.81);
        EXPECT_EQ(sample.timestamp_ns, 1'500'000'000ULL);
    }

    TEST(RosImuBridge, StatusValidForNormalValues)
    {
        sensor_msgs::msg::Imu msg;
        msg.angular_velocity.x = 1.0;
        msg.angular_velocity.y = 1.0;
        msg.angular_velocity.z = 1.0;

        const auto sample = RosImuBridge::to_sample(msg);

        using namespace safety_core::platform::sensors::imu_status;
        EXPECT_TRUE((sample.status & kValid) != 0U);
        EXPECT_FALSE((sample.status & kClipped) != 0U);
    }

    TEST(RosImuBridge, StatusClippedWhenAngularVelocityHigh)
    {
        sensor_msgs::msg::Imu msg;
        msg.angular_velocity.z = 35.0; // above 30 rad/s threshold

        const auto sample = RosImuBridge::to_sample(msg);

        using namespace safety_core::platform::sensors::imu_status;
        EXPECT_TRUE((sample.status & kClipped) != 0U);
    }

    TEST(RosImuBridge, UpdateFillsBufferedSensor)
    {
        using namespace safety_core::platform::sensors;
        sensor_msgs::msg::Imu msg;
        msg.angular_velocity.x = 2.5;
        msg.header.stamp.sec   = 5U;

        BufferedImuSensor sensor;
        RosImuBridge::update(sensor, msg);

        const auto sample = sensor.read();
        EXPECT_DOUBLE_EQ(sample.angular_velocity_x_radps, 2.5);
        EXPECT_EQ(sample.timestamp_ns, 5'000'000'000ULL);
    }

    // ---- RosOdometryBridge tests ---------------------------------------------------

    TEST(RosOdometryBridge, SampleFieldMapping)
    {
        nav_msgs::msg::Odometry msg;
        msg.header.stamp.sec     = 2U;
        msg.header.stamp.nanosec = 0U;
        msg.pose.pose.position.x = 3.5;
        msg.twist.twist.linear.x = 1.2;

        const auto sample = RosOdometryBridge::to_sample(msg);

        EXPECT_DOUBLE_EQ(sample.position_m, 3.5);
        EXPECT_DOUBLE_EQ(sample.velocity_mps, 1.2);
        EXPECT_EQ(sample.timestamp_ns, 2'000'000'000ULL);
    }

    TEST(RosOdometryBridge, IncrementFieldMapping)
    {
        nav_msgs::msg::Odometry msg;
        msg.twist.twist.linear.x  = 0.8;
        msg.twist.twist.angular.z = 0.5;
        msg.header.stamp.sec      = 3U;

        const auto inc = RosOdometryBridge::to_increment(msg);

        EXPECT_DOUBLE_EQ(inc.linear_velocity_mps, 0.8);
        EXPECT_DOUBLE_EQ(inc.angular_velocity_radps, 0.5);
        EXPECT_DOUBLE_EQ(inc.delta_x_m, 0.0); // dt not provided
        EXPECT_DOUBLE_EQ(inc.delta_y_m, 0.0);
        EXPECT_DOUBLE_EQ(inc.delta_yaw_rad, 0.0);
        EXPECT_EQ(inc.timestamp_ns, 3'000'000'000ULL);
    }

    TEST(RosOdometryBridge, UpdateFillsBufferedSensor)
    {
        using namespace safety_core::platform::sensors;
        nav_msgs::msg::Odometry msg;
        msg.pose.pose.position.x = 10.0;
        msg.twist.twist.linear.x = 2.0;
        msg.header.stamp.sec     = 7U;

        BufferedOdometrySensor sensor;
        RosOdometryBridge::update(sensor, msg);

        const auto sample = sensor.read();
        EXPECT_DOUBLE_EQ(sample.position_m, 10.0);
        EXPECT_DOUBLE_EQ(sample.velocity_mps, 2.0);

        const auto inc = sensor.read_increment();
        EXPECT_DOUBLE_EQ(inc.linear_velocity_mps, 2.0);
    }

    // ---- RosDriveActuatorBridge tests ---------------------------------------------

    TEST(RosDriveActuatorBridge, CommandAndLastCommand)
    {
        // nullptr publisher is safe — no publish, but last_command() updates
        RosDriveActuatorBridge bridge(nullptr);

        safety_core::platform::actuators::DriveCommand cmd;
        cmd.velocity_mps = 1.5;
        cmd.steering_rad = 0.3;
        cmd.timestamp_ns = 1'000'000'000ULL;

        bridge.command(cmd);

        const auto last = bridge.last_command();
        EXPECT_DOUBLE_EQ(last.velocity_mps, 1.5);
        EXPECT_DOUBLE_EQ(last.steering_rad, 0.3);
        EXPECT_EQ(last.timestamp_ns, 1'000'000'000ULL);
    }

    TEST(RosDriveActuatorBridge, StaleCommandDetection)
    {
        RosDriveActuatorBridge bridge(nullptr);

        safety_core::platform::actuators::DriveCommand cmd;
        cmd.timestamp_ns = 1'000'000'000ULL; // 1 second
        bridge.command(cmd);

        // is_command_fresh: now=2s, max_age=500ms → stale
        EXPECT_FALSE(bridge.is_command_fresh(2'000'000'000ULL, 500'000'000ULL));
        // is_command_fresh: now=1.2s, max_age=500ms → fresh
        EXPECT_TRUE(bridge.is_command_fresh(1'200'000'000ULL, 500'000'000ULL));
    }

} // namespace safety_core_ros
