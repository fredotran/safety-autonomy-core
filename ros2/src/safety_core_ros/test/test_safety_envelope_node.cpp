// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

// Integration tests for SafetyEnvelopeNode: drive the node with synthetic
// LaserScan + Odometry traffic and assert on the published EnvelopeStatus.
// These tests exercise the full scan -> corridor filter -> safety_core
// evaluate_stop_distance -> message mapping path.

#include "safety_core_ros/safety_envelope_node.hpp"

#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

using namespace std::chrono_literals;

namespace
{

    constexpr std::uint8_t ZONE_CLEAR      = 0U;
    constexpr std::uint8_t ZONE_WARNING    = 1U;
    constexpr std::uint8_t ZONE_PROTECTIVE = 2U;
    constexpr std::uint8_t ZONE_EMERGENCY  = 3U;

    class SafetyEnvelopeNodeTest : public ::testing::Test
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
            helper_ = std::make_shared<rclcpp::Node>("envelope_test_helper");

            // Reliable publishers satisfy the node's best_effort subscriptions and
            // behave like the Gazebo bridge / ros2cli publishers used in practice.
            // (A best_effort writer -> best_effort reader pair did not deliver
            // inside this test process under FastDDS.)
            const auto sensor_qos = rclcpp::QoS(rclcpp::KeepLast(5)).reliable().durability_volatile();
            scan_pub_             = helper_->create_publisher<sensor_msgs::msg::LaserScan>("scan", sensor_qos);
            odom_pub_             = helper_->create_publisher<nav_msgs::msg::Odometry>("odom", sensor_qos);

            status_sub_ = helper_->create_subscription<safety_core_msgs::msg::EnvelopeStatus>(
                "safety/envelope_status", rclcpp::QoS(rclcpp::KeepLast(10)),
                [this](const safety_core_msgs::msg::EnvelopeStatus::ConstSharedPtr msg)
                {
                    last_status_ = *msg;
                    have_status_ = true;
                });
        }

        void make_sut(const std::vector<rclcpp::Parameter>& overrides = {})
        {
            rclcpp::NodeOptions options;
            options.parameter_overrides(overrides);
            // Executor holds a weak_ptr — the node must outlive the test body.
            sut_ = std::make_shared<safety_core_ros::SafetyEnvelopeNode>(options);
            exec_.add_node(sut_);
            exec_.add_node(helper_);
        }

        sensor_msgs::msg::LaserScan make_forward_scan(double obstacle_distance_m)
        {
            // 8 rays over the full circle; only the angle-0 ray hits the forward
            // corridor (y == 0 < corridor). Other rays carry +inf.
            sensor_msgs::msg::LaserScan scan;
            scan.header.frame_id = "lidar_link";
            scan.header.stamp    = helper_->now();
            scan.angle_min       = 0.0F;
            scan.angle_max       = 2.0F * static_cast<float>(M_PI);
            scan.angle_increment = static_cast<float>(M_PI_4);
            scan.range_min       = 0.05F;
            scan.range_max       = 30.0F;
            const float obstacle = static_cast<float>(obstacle_distance_m);
            const float inf      = std::numeric_limits<float>::infinity();
            scan.ranges          = {obstacle, inf, inf, inf, inf, inf, inf, inf};
            return scan;
        }

        // Deliver `speed_mps` through /odom before any scan is sent, so the first
        // envelope evaluation already sees the intended speed (topic ordering
        // across different subscriptions is not guaranteed within one spin).
        void prime_odom(double speed_mps)
        {
            nav_msgs::msg::Odometry odom;
            odom.twist.twist.linear.x = speed_mps;
            for (int i = 0; i < 10; ++i)
            {
                odom_pub_->publish(odom);
                exec_.spin_some(20ms);
            }
        }

        // Publish `scan` until an EnvelopeStatus arrives (or deadline expires).
        // Volatile QoS means early publishes can be lost before discovery, so the
        // scan is republished periodically.
        bool run_until_status(const sensor_msgs::msg::LaserScan& scan, double speed_mps = 0.0,
                              std::chrono::seconds budget = 5s)
        {
            have_status_ = false;

            nav_msgs::msg::Odometry odom;
            odom.twist.twist.linear.x = speed_mps;

            const auto deadline = std::chrono::steady_clock::now() + budget;
            while (!have_status_ && std::chrono::steady_clock::now() < deadline)
            {
                odom_pub_->publish(odom);
                scan_pub_->publish(scan);
                exec_.spin_some(50ms);
            }
            return have_status_;
        }

        rclcpp::executors::SingleThreadedExecutor exec_;
        rclcpp::Node::SharedPtr helper_;
        std::shared_ptr<safety_core_ros::SafetyEnvelopeNode> sut_;
        rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub_;
        rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
        rclcpp::Subscription<safety_core_msgs::msg::EnvelopeStatus>::SharedPtr status_sub_;
        safety_core_msgs::msg::EnvelopeStatus last_status_{};
        bool have_status_{false};
    };

    TEST_F(SafetyEnvelopeNodeTest, NoObstaclePublishesInvalidDistanceAndWarning)
    {
        // Past the grace period, absence of valid returns must NOT read as Clear —
        // the node defaults to Warning so downstream treats unknown as unsafe.
        make_sut({rclcpp::Parameter("startup_grace_period_s", 0.0)});
        ASSERT_TRUE(run_until_status(make_forward_scan(std::numeric_limits<double>::infinity())));
        EXPECT_DOUBLE_EQ(last_status_.distance_to_obstacle_m, -1.0);
        EXPECT_EQ(last_status_.zone.zone, ZONE_WARNING);
        EXPECT_FALSE(last_status_.within_envelope);
    }

    TEST_F(SafetyEnvelopeNodeTest, DistantObstaclePublishesClearZone)
    {
        make_sut({rclcpp::Parameter("startup_grace_period_s", 0.0)});
        ASSERT_TRUE(run_until_status(make_forward_scan(5.0)));
        EXPECT_NEAR(last_status_.distance_to_obstacle_m, 5.0, 1e-6);
        EXPECT_EQ(last_status_.zone.zone, ZONE_CLEAR);
        EXPECT_TRUE(last_status_.within_envelope);
    }

    TEST_F(SafetyEnvelopeNodeTest, CloseObstaclePublishesWarningZone)
    {
        // 0.52 m raw -> 0.42 m effective after the 0.1 m front overhang. Warning
        // zone spans [buffer, 1.5*required) = [0.30, 0.45) effective; above that
        // is Clear. (0.6 m raw would still be Clear — boundary precision matters.)
        make_sut({rclcpp::Parameter("startup_grace_period_s", 0.0)});
        ASSERT_TRUE(run_until_status(make_forward_scan(0.52)));
        EXPECT_NEAR(last_status_.distance_to_obstacle_m, 0.52, 1e-6);
        EXPECT_EQ(last_status_.zone.zone, ZONE_WARNING);
    }

    TEST_F(SafetyEnvelopeNodeTest, VeryCloseObstaclePublishesEmergencyZone)
    {
        // Lower the ignore range so a 0.35 m return is counted: effective 0.25 m
        // is inside the 0.3 m safety buffer -> Emergency, zero speed limit.
        make_sut({rclcpp::Parameter("startup_grace_period_s", 0.0), rclcpp::Parameter("scan_ignore_min_range_m", 0.3)});
        ASSERT_TRUE(run_until_status(make_forward_scan(0.35)));
        EXPECT_NEAR(last_status_.distance_to_obstacle_m, 0.35, 1e-6);
        EXPECT_EQ(last_status_.zone.zone, ZONE_EMERGENCY);
        EXPECT_DOUBLE_EQ(last_status_.recommended_speed_limit_mps, 0.0);
    }

    TEST_F(SafetyEnvelopeNodeTest, SpeedFromOdomReducesToProtective)
    {
        // Same 0.6 m geometry but moving at 1.0 m/s: required clearance becomes
        // 0.04 + 0.5 + 0.3 = 0.84 m > effective 0.5 m -> Protective.
        make_sut({rclcpp::Parameter("startup_grace_period_s", 0.0)});
        prime_odom(1.0);
        ASSERT_TRUE(run_until_status(make_forward_scan(0.6), /*speed_mps=*/1.0));
        EXPECT_DOUBLE_EQ(last_status_.current_speed_mps, 1.0);
        EXPECT_EQ(last_status_.zone.zone, ZONE_PROTECTIVE);
        EXPECT_FALSE(last_status_.within_envelope);
    }

    TEST_F(SafetyEnvelopeNodeTest, ObstacleOutsideForwardCorridorIsIgnored)
    {
        // Single return at 60 deg: x = 0.5 m forward, y = 0.87 m lateral — outside
        // the 0.8 m corridor, so the obstacle must not count.
        sensor_msgs::msg::LaserScan scan;
        scan.header.frame_id = "lidar_link";
        scan.header.stamp    = helper_->now();
        scan.angle_min       = static_cast<float>(M_PI / 3.0);
        scan.angle_max       = static_cast<float>(M_PI / 3.0);
        scan.angle_increment = static_cast<float>(M_PI / 3.0);
        scan.range_min       = 0.05F;
        scan.range_max       = 30.0F;
        scan.ranges          = {1.0F};

        make_sut({rclcpp::Parameter("startup_grace_period_s", 0.0)});
        ASSERT_TRUE(run_until_status(scan));
        EXPECT_DOUBLE_EQ(last_status_.distance_to_obstacle_m, -1.0);
        EXPECT_EQ(last_status_.zone.zone, ZONE_WARNING);
    }

    TEST_F(SafetyEnvelopeNodeTest, GracePeriodDowngradesEmergencyToWarning)
    {
        // Inside the startup grace window an Emergency evaluation is softened to
        // Warning so the stack does not latch faults while sensors stabilize.
        make_sut(
            {rclcpp::Parameter("startup_grace_period_s", 600.0), rclcpp::Parameter("scan_ignore_min_range_m", 0.3)});
        ASSERT_TRUE(run_until_status(make_forward_scan(0.35)));
        // distance proves the 0.35 m return was counted (i.e. the ignore-range
        // parameter override reached the node).
        EXPECT_NEAR(last_status_.distance_to_obstacle_m, 0.35, 1e-6);
        EXPECT_EQ(last_status_.zone.zone, ZONE_WARNING);
    }

    TEST_F(SafetyEnvelopeNodeTest, StatusCarriesFootprintGeometry)
    {
        make_sut({rclcpp::Parameter("startup_grace_period_s", 0.0)});
        ASSERT_TRUE(run_until_status(make_forward_scan(5.0)));
        EXPECT_DOUBLE_EQ(last_status_.footprint_length_m, 0.8);
        EXPECT_DOUBLE_EQ(last_status_.footprint_width_m, 0.6);
        EXPECT_DOUBLE_EQ(last_status_.footprint_front_overhang_m, 0.1);
    }

} // namespace
