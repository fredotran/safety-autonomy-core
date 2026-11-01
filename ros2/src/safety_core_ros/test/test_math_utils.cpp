// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/math_utils.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

using safety_core_ros::MathUtils;

TEST(MathUtils, IsInRangeBoundariesInclusive)
{
    EXPECT_TRUE(MathUtils::is_in_range(0.5, 0.0, 1.0));
    EXPECT_TRUE(MathUtils::is_in_range(0.0, 0.0, 1.0));
    EXPECT_TRUE(MathUtils::is_in_range(1.0, 0.0, 1.0));
    EXPECT_FALSE(MathUtils::is_in_range(-0.001, 0.0, 1.0));
    EXPECT_FALSE(MathUtils::is_in_range(1.001, 0.0, 1.0));
}

TEST(MathUtils, ClampRespectsBounds)
{
    EXPECT_DOUBLE_EQ(MathUtils::clamp(0.5, 0.0, 1.0), 0.5);
    EXPECT_DOUBLE_EQ(MathUtils::clamp(-1.0, 0.0, 1.0), 0.0);
    EXPECT_DOUBLE_EQ(MathUtils::clamp(2.0, 0.0, 1.0), 1.0);
}

TEST(MathUtils, IsFiniteRejectsNonFinite)
{
    EXPECT_TRUE(MathUtils::is_finite(0.0));
    EXPECT_TRUE(MathUtils::is_finite(-1e300));
    EXPECT_FALSE(MathUtils::is_finite(std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(MathUtils::is_finite(-std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(MathUtils::is_finite(std::numeric_limits<double>::quiet_NaN()));
}

TEST(MathUtils, SquaredDistance)
{
    EXPECT_DOUBLE_EQ(MathUtils::squared_distance(3.0, 4.0), 25.0);
    EXPECT_DOUBLE_EQ(MathUtils::squared_distance(0.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(MathUtils::squared_distance(-1.0, 1.0), 2.0);
}

TEST(MathUtils, CorridorChecksAgree)
{
    const double half_width    = 0.8;
    const double half_width_sq = half_width * half_width;

    for (double y = -1.0; y <= 1.0; y += 0.01)
    {
        EXPECT_EQ(MathUtils::is_in_corridor(y, half_width), MathUtils::is_in_corridor_squared(y, half_width_sq))
            << "y=" << y;
    }
}

TEST(MathUtils, DegRadRoundTrip)
{
    EXPECT_DOUBLE_EQ(MathUtils::degrees_to_radians(180.0), M_PI);
    EXPECT_DOUBLE_EQ(MathUtils::degrees_to_radians(0.0), 0.0);
    EXPECT_NEAR(MathUtils::radians_to_degrees(MathUtils::degrees_to_radians(42.5)), 42.5, 1e-12);
}

TEST(MathUtils, LerpEndpointsAndMidpoint)
{
    EXPECT_DOUBLE_EQ(MathUtils::lerp(10.0, 20.0, 0.0), 10.0);
    EXPECT_DOUBLE_EQ(MathUtils::lerp(10.0, 20.0, 1.0), 20.0);
    EXPECT_DOUBLE_EQ(MathUtils::lerp(10.0, 20.0, 0.5), 15.0);
    // Extrapolation is allowed (no clamping documented)
    EXPECT_DOUBLE_EQ(MathUtils::lerp(10.0, 20.0, 1.5), 25.0);
}
