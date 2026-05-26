// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/math_utils.hpp"
#include "test_helpers.hpp"

#include <gtest/gtest.h>

using safety_core_ros::math::clamp;
using safety_core_ros::math::deadband;
using safety_core_ros::math::exponential_decay;
using safety_core_test::approx_equal;

TEST(MathUtils, ClampWithinRange)
{
    EXPECT_DOUBLE_EQ(clamp(5.0, 0.0, 10.0), 5.0);
    EXPECT_DOUBLE_EQ(clamp(0.0, 0.0, 10.0), 0.0);
    EXPECT_DOUBLE_EQ(clamp(10.0, 0.0, 10.0), 10.0);
}

TEST(MathUtils, ClampBelowRange)
{
    EXPECT_DOUBLE_EQ(clamp(-5.0, 0.0, 10.0), 0.0);
    EXPECT_DOUBLE_EQ(clamp(-100.0, -50.0, 50.0), -50.0);
}

TEST(MathUtils, ClampAboveRange)
{
    EXPECT_DOUBLE_EQ(clamp(15.0, 0.0, 10.0), 10.0);
    EXPECT_DOUBLE_EQ(clamp(100.0, -50.0, 50.0), 50.0);
}

TEST(MathUtils, DeadbandWithinBand)
{
    EXPECT_DOUBLE_EQ(deadband(0.0, 1.0), 0.0);
    EXPECT_DOUBLE_EQ(deadband(0.5, 1.0), 0.0);
    EXPECT_DOUBLE_EQ(deadband(-0.5, 1.0), 0.0);
}

TEST(MathUtils, DeadbandOutsideBand)
{
    EXPECT_DOUBLE_EQ(deadband(1.5, 1.0), 1.5);
    EXPECT_DOUBLE_EQ(deadband(-1.5, 1.0), -1.5);
    EXPECT_DOUBLE_EQ(deadband(2.0, 1.0), 2.0);
}

TEST(MathUtils, ExponentialDecayZeroDt)
{
    // With zero dt, no decay → value stays the same
    EXPECT_TRUE(approx_equal(exponential_decay(10.0, 0.0, 1.0), 10.0));
    EXPECT_TRUE(approx_equal(exponential_decay(5.0, 0.0, 2.0), 5.0));
}

TEST(MathUtils, ExponentialDecayFullTimeConstant)
{
    // After one time constant (tau), value decays to ~36.8% (1/e)
    const double initial = 10.0;
    const double tau     = 1.0;
    const double result  = exponential_decay(initial, tau, tau);
    EXPECT_TRUE(approx_equal(result, initial * 0.36787944117, 1e-6));
}

TEST(MathUtils, ExponentialDecayLargeDt)
{
    // After many time constants, value approaches zero
    const double result = exponential_decay(10.0, 10.0, 1.0);
    EXPECT_TRUE(approx_equal(result, 0.0, 1e-3));
}
