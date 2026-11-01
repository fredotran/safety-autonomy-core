// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/time_utils.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <rclcpp/rclcpp.hpp>

using safety_core_ros::TimeUtils;

TEST(TimeUtils, SecondsToNanoseconds)
{
    EXPECT_EQ(TimeUtils::seconds_to_nanoseconds(0.0), 0U);
    EXPECT_EQ(TimeUtils::seconds_to_nanoseconds(1.0), 1'000'000'000U);
    EXPECT_EQ(TimeUtils::seconds_to_nanoseconds(1.5), 1'500'000'000U);
}

TEST(TimeUtils, NanosecondsToSeconds)
{
    EXPECT_DOUBLE_EQ(TimeUtils::nanoseconds_to_seconds(0U), 0.0);
    EXPECT_DOUBLE_EQ(TimeUtils::nanoseconds_to_seconds(1'000'000'000U), 1.0);
    EXPECT_DOUBLE_EQ(TimeUtils::nanoseconds_to_seconds(500'000'000U), 0.5);
}

TEST(TimeUtils, NowNanosecondsReturnsNonZero)
{
    auto clock = std::make_shared<rclcpp::Clock>(RCL_SYSTEM_TIME);
    EXPECT_GT(TimeUtils::now_nanoseconds(clock), 0U);
}

TEST(TimeUtils, CalculateAgeNs)
{
    EXPECT_EQ(TimeUtils::calculate_age_ns(100U, 250U), 150U);
    EXPECT_EQ(TimeUtils::calculate_age_ns(0U, 0U), 0U);
}

TEST(TimeUtils, IsStaleThresholdIsExclusive)
{
    // age == timeout must NOT be stale (strict >)
    EXPECT_FALSE(TimeUtils::is_stale(100U, 200U, 100U));
    EXPECT_TRUE(TimeUtils::is_stale(100U, 201U, 100U));
    EXPECT_FALSE(TimeUtils::is_stale(100U, 199U, 100U));
}

TEST(TimeUtils, TimestampInFutureUnderflows)
{
    // current < timestamp wraps to a huge age -> reported stale.
    // Documents existing behaviour: callers must guarantee monotonicity.
    EXPECT_TRUE(TimeUtils::is_stale(200U, 100U, 50U));
}
