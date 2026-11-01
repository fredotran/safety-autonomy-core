// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT
//
// End-to-end scenario regression: real SafetyEnvelopeNode + SafetySupervisorNode
// in one process, driven by synthetic LaserScan/Odometry. Asserts the behavioral
// chain the demos rely on — clear drive, protective hold, pop-up obstacle ->
// latched SafeStop — with measurable deadlines.
//
// @req SR-SENSE-001  scan stamp validation + stream watchdog -> EMERGENCY
// @req SR-SENSE-002  envelope_status stream loss -> fault 0xE002 -> SafeStop
// @req SR-SENSE-003  stale/missing odometry -> conservative speed, LocalizationLost
//
// Scenarios:
//   A. Clear path          -> zone CLEAR, supervisor Idle->Moving
//   B. Pop-up obstacle     -> zone EMERGENCY -> SafeStop latched < 2 s
//   C. Protective slowdown -> obstacle hold (AvoidingObstacle), then recover

#include "safety_core_ros/safety_envelope_node.hpp"
#include "safety_core_ros/safety_supervisor_node.hpp"

#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <safety_core_msgs/msg/safety_state.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <std_msgs/msg/bool.hpp>

using namespace std::chrono_literals;

namespace
{

    constexpr std::uint8_t ZONE_CLEAR      = 0U;
    constexpr std::uint8_t ZONE_WARNING    = 1U;
    constexpr std::uint8_t ZONE_PROTECTIVE = 2U;
    constexpr std::uint8_t ZONE_EMERGENCY  = 3U;

    constexpr std::uint8_t MODE_IDLE              = 1U;
    constexpr std::uint8_t MODE_MOVING            = 2U;
    constexpr std::uint8_t MODE_AVOIDING_OBSTACLE = 4U;
    constexpr std::uint8_t MODE_LOCALIZATION_LOST = 5U;
    constexpr std::uint8_t MODE_SAFE_STOP         = 7U;

    class SafetyScenarioTest : public ::testing::Test
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
            helper_ = std::make_shared<rclcpp::Node>("scenario_test_helper");

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

            state_sub_ = helper_->create_subscription<safety_core_msgs::msg::SafetyState>(
                "safety/state", rclcpp::QoS(rclcpp::KeepLast(10)),
                [this](const safety_core_msgs::msg::SafetyState::ConstSharedPtr msg)
                {
                    last_state_ = *msg;
                    have_state_ = true;
                });

            stop_sub_ =
                helper_->create_subscription<std_msgs::msg::Bool>("safety/safe_stop", rclcpp::QoS(rclcpp::KeepLast(10)),
                                                                  [this](const std_msgs::msg::Bool::ConstSharedPtr msg)
                                                                  {
                                                                      if (msg->data)
                                                                      {
                                                                          saw_safe_stop_true_ = true;
                                                                      }
                                                                  });
        }

        // Both SUTs share the fixture executor; strong ownership retained here.
        void make_stack()
        {
            rclcpp::NodeOptions envelope_options;
            // scan_timeout must comfortably exceed DDS endpoint discovery
            // (~0.5-1 s in-container) or the watchdog fires before the first
            // fresh scan is ever delivered.
            envelope_options.parameter_overrides({rclcpp::Parameter("startup_grace_period_s", 0.0),
                                                  rclcpp::Parameter("scan_ignore_min_range_m", 0.3),
                                                  rclcpp::Parameter("scan_timeout_s", 2.0)});
            envelope_node_ = std::make_shared<safety_core_ros::SafetyEnvelopeNode>(envelope_options);

            supervisor_node_ = std::make_shared<safety_core_ros::SafetySupervisorNode>(rclcpp::NodeOptions{});

            exec_.add_node(envelope_node_);
            exec_.add_node(supervisor_node_);
            exec_.add_node(helper_);
        }

        sensor_msgs::msg::LaserScan make_forward_scan(double obstacle_distance_m)
        {
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

        // Continuously pump scan + odom while spinning; returns the wall-clock
        // milliseconds until `predicate` holds (or -1 on deadline expiry).
        template <typename Pred>
        double run_until(const sensor_msgs::msg::LaserScan& scan, double speed_mps, Pred&& predicate,
                         std::chrono::milliseconds deadline)
        {
            nav_msgs::msg::Odometry odom;
            odom.twist.twist.linear.x = speed_mps;

            const auto t0     = std::chrono::steady_clock::now();
            const auto expire = t0 + deadline;
            while (!predicate() && std::chrono::steady_clock::now() < expire)
            {
                // The supervisor derives localization staleness from the odom
                // header stamp, and the envelope node rejects unstamped or
                // stale scans — keep both fresh or the system escalates.
                odom.header.stamp       = helper_->now();
                auto fresh_scan         = scan;
                fresh_scan.header.stamp = helper_->now();
                odom_pub_->publish(odom);
                scan_pub_->publish(fresh_scan);
                exec_.spin_some(20ms);
            }
            if (!predicate())
            {
                return -1.0;
            }
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        }

        rclcpp::executors::SingleThreadedExecutor exec_;
        rclcpp::Node::SharedPtr helper_;
        std::shared_ptr<safety_core_ros::SafetyEnvelopeNode> envelope_node_;
        std::shared_ptr<safety_core_ros::SafetySupervisorNode> supervisor_node_;
        rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub_;
        rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
        rclcpp::Subscription<safety_core_msgs::msg::EnvelopeStatus>::SharedPtr status_sub_;
        rclcpp::Subscription<safety_core_msgs::msg::SafetyState>::SharedPtr state_sub_;
        rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr stop_sub_;
        safety_core_msgs::msg::EnvelopeStatus last_status_{};
        safety_core_msgs::msg::SafetyState last_state_{};
        bool have_status_{false};
        bool have_state_{false};
        bool saw_safe_stop_true_{false};
    };

    // Scenario A: clear path -> CLEAR zone and supervisor auto-Moving.
    TEST_F(SafetyScenarioTest, ClearPathReachesMoving)
    {
        make_stack();
        const double elapsed = run_until(
            make_forward_scan(5.0), 0.0,
            [this] {
                return have_status_ && last_status_.zone.zone == ZONE_CLEAR && have_state_ &&
                       last_state_.mode == MODE_MOVING;
            },
            10s);
        ASSERT_GE(elapsed, 0.0) << "supervisor never reached Moving on a clear path";
        EXPECT_FALSE(last_state_.fault_latched);
        std::printf("[scenario] clear->Moving latency: %.1f ms\n", elapsed);
    }

    // Scenario B: pop-up obstacle inside the buffer -> EMERGENCY -> SafeStop.
    TEST_F(SafetyScenarioTest, PopupObstacleLatchesSafeStopWithinDeadline)
    {
        make_stack();

        // Drive to Moving first (clear path), then teleport an obstacle.
        ASSERT_GE(
            run_until(
                make_forward_scan(5.0), 0.0, [this] { return have_state_ && last_state_.mode == MODE_MOVING; }, 10s),
            0.0)
            << "precondition failed: supervisor did not reach Moving";

        saw_safe_stop_true_  = false;
        const double elapsed = run_until(
            make_forward_scan(0.35), 0.0,
            [this] { return have_state_ && last_state_.mode == MODE_SAFE_STOP && last_state_.fault_latched; }, 2s);
        ASSERT_GE(elapsed, 0.0) << "SafeStop not latched within 2 s of emergency scan";
        EXPECT_TRUE(last_state_.safe_stop_requested);
        EXPECT_EQ(last_state_.fault_code, 0xE001U);
        EXPECT_TRUE(saw_safe_stop_true_) << "/safety/safe_stop=true was never published";
        std::printf("[scenario] emergency scan -> latched SafeStop: %.1f ms\n", elapsed);
    }

    // Scenario C: protective zone at speed -> obstacle hold, then auto-recover
    // once the path clears.
    TEST_F(SafetyScenarioTest, ProtectiveHoldsThenRecovers)
    {
        make_stack();

        // Reach Moving with speed so protective threshold is meaningful.
        ASSERT_GE(
            run_until(
                make_forward_scan(5.0), 1.0, [this] { return have_state_ && last_state_.mode == MODE_MOVING; }, 10s),
            0.0)
            << "precondition failed: supervisor did not reach Moving";

        // 0.6 m at 1.0 m/s -> required clearance ~0.84 m -> Protective.
        ASSERT_GE(run_until(
                      make_forward_scan(0.6), 1.0,
                      [this]
                      {
                          return have_status_ && last_status_.zone.zone == ZONE_PROTECTIVE && have_state_ &&
                                 last_state_.mode == MODE_AVOIDING_OBSTACLE;
                      },
                      5s),
                  0.0)
            << "protective zone did not trigger obstacle hold";

        // Path clears -> auto-recover to Moving.
        ASSERT_GE(
            run_until(
                make_forward_scan(5.0), 0.0, [this] { return have_state_ && last_state_.mode == MODE_MOVING; }, 5s),
            0.0)
            << "cleared path did not auto-recover to Moving";
    }

    // Scenario D (negative): a warning-zone obstacle must NOT latch a fault.
    TEST_F(SafetyScenarioTest, WarningZoneDoesNotLatchFault)
    {
        make_stack();
        // 0.52 m stationary -> effective 0.42 m -> Warning.
        ASSERT_GE(run_until(
                      make_forward_scan(0.52), 0.0,
                      [this] { return have_status_ && last_status_.zone.zone == ZONE_WARNING; }, 10s),
                  0.0)
            << "warning zone never reported";

        // Hold the warning for ~1 s of spinning; no fault must latch.
        nav_msgs::msg::Odometry odom;
        auto hold_scan      = make_forward_scan(0.52);
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (std::chrono::steady_clock::now() < deadline)
        {
            odom.header.stamp = helper_->now();
            odom_pub_->publish(odom);
            hold_scan.header.stamp = helper_->now();
            scan_pub_->publish(hold_scan);
            exec_.spin_some(20ms);
        }
        EXPECT_FALSE(last_state_.fault_latched) << "warning zone latched a fault";
        EXPECT_NE(last_state_.mode, MODE_SAFE_STOP) << "warning zone forced SafeStop";
        EXPECT_FALSE(saw_safe_stop_true_);
    }

    // A LiDAR frame with an old stamp (frozen driver, replayed bag data) must
    // be rejected as stale — never evaluated as current obstacle data.
    TEST_F(SafetyScenarioTest, StaleScanStampTriggersEmergency)
    {
        make_stack();
        ASSERT_GE(
            run_until(
                make_forward_scan(5.0), 0.0, [this] { return have_state_ && last_state_.mode == MODE_MOVING; }, 10s),
            0.0)
            << "never reached Moving on clear path | have_status=" << have_status_
            << " zone=" << static_cast<int>(last_status_.zone.zone) << " have_state=" << have_state_
            << " mode=" << static_cast<int>(last_state_.mode) << " fault=" << last_state_.fault_latched << " fcode=0x"
            << std::hex << last_state_.fault_code;

        // Keep odom fresh (localization stays healthy) but feed a scan whose
        // stamp is 3 s old — well past the fixture's scan_timeout_s (2.0 s).
        nav_msgs::msg::Odometry odom;
        auto stale         = make_forward_scan(5.0);
        stale.header.stamp = helper_->now() - rclcpp::Duration::from_seconds(3.0);
        const auto t0      = std::chrono::steady_clock::now();
        while (!last_state_.fault_latched && std::chrono::steady_clock::now() - t0 < 3s)
        {
            odom.header.stamp = helper_->now();
            odom_pub_->publish(odom);
            scan_pub_->publish(stale);
            exec_.spin_some(20ms);
        }

        EXPECT_TRUE(last_state_.fault_latched) << "stale scan never escalated";
        EXPECT_EQ(last_state_.mode, MODE_SAFE_STOP);
        EXPECT_EQ(last_state_.fault_code, 0xE001U);
    }

    // Total scan stream loss must be caught by the envelope watchdog even
    // though no callback fires — EMERGENCY status -> latched SafeStop.
    TEST_F(SafetyScenarioTest, ScanStreamLossTriggersSafeStop)
    {
        make_stack();
        ASSERT_GE(
            run_until(
                make_forward_scan(5.0), 0.0, [this] { return have_state_ && last_state_.mode == MODE_MOVING; }, 10s),
            0.0)
            << "never reached Moving on clear path";

        // Stop publishing scans; keep odom flowing so only the scan path fails.
        // Latch expected ~scan_timeout (2 s) + watchdog period (0.5 s).
        nav_msgs::msg::Odometry odom;
        const auto t0 = std::chrono::steady_clock::now();
        while (!last_state_.fault_latched && std::chrono::steady_clock::now() - t0 < 6s)
        {
            odom.header.stamp = helper_->now();
            odom_pub_->publish(odom);
            exec_.spin_some(20ms);
        }

        EXPECT_TRUE(last_state_.fault_latched)
            << "silent scan stream never escalated | have_status=" << have_status_
            << " zone=" << static_cast<int>(last_status_.zone.zone) << " have_state=" << have_state_
            << " mode=" << static_cast<int>(last_state_.mode) << " fault=" << last_state_.fault_latched;
        EXPECT_EQ(last_state_.mode, MODE_SAFE_STOP);
    }

    // If the envelope node itself dies, the supervisor-side staleness check
    // latches fault 0xE002 — last line of defense.
    TEST_F(SafetyScenarioTest, EnvelopeNodeLossLatchesFaultE002)
    {
        make_stack();
        ASSERT_GE(
            run_until(
                make_forward_scan(5.0), 0.0, [this] { return have_state_ && last_state_.mode == MODE_MOVING; }, 10s),
            0.0)
            << "never reached Moving on clear path";

        // Simulate node death: drop it from the executor so it stops
        // publishing envelope statuses entirely.
        exec_.remove_node(envelope_node_);
        envelope_node_.reset();

        nav_msgs::msg::Odometry odom;
        const auto t0 = std::chrono::steady_clock::now();
        while (!last_state_.fault_latched && std::chrono::steady_clock::now() - t0 < 4s)
        {
            odom.header.stamp = helper_->now();
            odom_pub_->publish(odom);
            exec_.spin_some(20ms);
        }

        EXPECT_TRUE(last_state_.fault_latched) << "dead envelope node never escalated";
        EXPECT_EQ(last_state_.mode, MODE_SAFE_STOP);
        EXPECT_EQ(last_state_.fault_code, 0xE002U) << "expected envelope-staleness fault code";
    }

    // A frozen odometry stream must not freeze the speed input at its last
    // value — an odom stuck at 0 m/s while the robot moves would make the
    // envelope *underestimate* stopping distance. Fail-safe: assume worst
    // case (max speed) once the stream is stale.
    TEST_F(SafetyScenarioTest, StaleOdomAssumesMaxSpeedForEnvelope)
    {
        make_stack();
        // Obstacle at 0.9 m: CLEAR while reported speed is 0, but elevated
        // once evaluation falls back to max_speed_mps (1.5 m/s default).
        ASSERT_GE(
            run_until(
                make_forward_scan(0.9), 0.0, [this] { return have_state_ && last_state_.mode == MODE_MOVING; }, 10s),
            0.0)
            << "never reached Moving on clear path";

        ASSERT_TRUE(have_status_);
        const auto zone_before = last_status_.zone.zone;

        // Stop odometry; keep scans flowing with fresh stamps.
        nav_msgs::msg::Odometry odom;
        auto scan               = make_forward_scan(0.9);
        const auto t0           = std::chrono::steady_clock::now();
        bool degraded_zone_seen = false;
        while (std::chrono::steady_clock::now() - t0 < 4s)
        {
            // NOTE: no odom_pub_->publish — stream intentionally dead.
            scan.header.stamp = helper_->now();
            scan_pub_->publish(scan);
            exec_.spin_some(20ms);
            if (have_status_ && last_status_.zone.zone > zone_before)
            {
                degraded_zone_seen = true;
                break;
            }
        }
        EXPECT_TRUE(degraded_zone_seen) << "stale odometry did not degrade zone | last zone="
                                        << static_cast<int>(last_status_.zone.zone)
                                        << " limit=" << last_status_.recommended_speed_limit_mps;
    }

    // Odometry that NEVER arrives is stale-by-definition: the supervisor must
    // reach LocalizationLost from the startup baseline, not wait forever for
    // a first sample.
    TEST_F(SafetyScenarioTest, MissingOdomFromBootTriggersLocalizationLost)
    {
        make_stack();
        // Publish clear scans only — no odometry ever.
        auto scan     = make_forward_scan(5.0);
        const auto t0 = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - t0 < 4s)
        {
            scan.header.stamp = helper_->now();
            scan_pub_->publish(scan);
            exec_.spin_some(20ms);
            if (have_state_ && last_state_.mode == MODE_LOCALIZATION_LOST)
            {
                break;
            }
        }
        EXPECT_EQ(last_state_.mode, MODE_LOCALIZATION_LOST)
            << "never-received odometry never escalated | mode=" << static_cast<int>(last_state_.mode);
        EXPECT_FALSE(last_state_.fault_latched) << "localization loss must be a recoverable mode, not a latch";
    }

} // namespace
