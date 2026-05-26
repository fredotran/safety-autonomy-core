// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/time_utils.hpp"
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using safety_core_ros::TimeUtils;
using safety_core_test::approx_equal;

TEST(TimeUtils, SecondsToNanoseconds)
{
    EXPECT_EQ(TimeUtils::seconds_to_nanoseconds(0.0), 0ULL);
    EXPECT_EQ(TimeUtils::seconds_to_nanoseconds(1.0), 1'000'000'000ULL);
    EXPECT_EQ(TimeUtils::seconds_to_nanoseconds(0.5), 500'000'000ULL);
    EXPECT_EQ(TimeUtils::seconds_to_nanoseconds(2.5), 2'500'000'000ULL);
}

TEST(TimeUtils, NanosecondsToSeconds)
{
    EXPECT_TRUE(approx_equal(TimeUtils::nanoseconds_to_seconds(0ULL), 0.0));
    EXPECT_TRUE(approx_equal(TimeUtils::nanoseconds_to_seconds(1'000'000'000ULL), 1.0));
    EXPECT_TRUE(approx_equal(TimeUtils::nanoseconds_to_seconds(500'000'000ULL), 0.5));
    EXPECT_TRUE(approx_equal(TimeUtils::nanoseconds_to_seconds(2'500'000'000ULL), 2.5));
}

TEST(TimeUtils, RoundTripConversion)
{
    const double original_seconds  = 3.14159;
    const std::uint64_t ns         = TimeUtils::seconds_to_nanoseconds(original_seconds);
    const double recovered_seconds = TimeUtils::nanoseconds_to_seconds(ns);
    EXPECT_TRUE(approx_equal(original_seconds, recovered_seconds, 1e-7));
}

TEST(TimeUtils, CalculateAge)
{
    EXPECT_EQ(TimeUtils::calculate_age_ns(0ULL, 1'000'000'000ULL), 1'000'000'000ULL);
    EXPECT_EQ(TimeUtils::calculate_age_ns(500'000'000ULL, 1'500'000'000ULL), 1'000'000'000ULL);
    EXPECT_EQ(TimeUtils::calculate_age_ns(1'000'000'000ULL, 1'000'000'000ULL), 0ULL);
}

TEST(TimeUtils, IsStale)
{
    // Exactly at timeout boundary (not stale)
    EXPECT_FALSE(TimeUtils::is_stale(0ULL, 1'000'000'000ULL, 1'000'000'000ULL));
    // Just over timeout (stale)
    EXPECT_TRUE(TimeUtils::is_stale(0ULL, 1'000'000'001ULL, 1'000'000'000ULL));
    // Well under timeout (not stale)
    EXPECT_FALSE(TimeUtils::is_stale(0ULL, 500'000'000ULL, 1'000'000'000ULL));
}

TEST(TimeUtils, IsStaleWithOffset)
{
    const std::uint64_t timestamp = 1'000'000'000ULL;
    const std::uint64_t timeout   = 500'000'000ULL;

    // Age = 499ms (< 500ms timeout) → not stale
    EXPECT_FALSE(TimeUtils::is_stale(timestamp, timestamp + 499'000'000ULL, timeout));
    // Age = 500ms (== 500ms timeout) → not stale
    EXPECT_FALSE(TimeUtils::is_stale(timestamp, timestamp + 500'000'000ULL, timeout));
    // Age = 501ms (> 500ms timeout) → stale
    EXPECT_TRUE(TimeUtils::is_stale(timestamp, timestamp + 501'000'000ULL, timeout));
}
