// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core_ros/param_loader.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

using safety_core_ros::ParamLoader;

class ParamLoaderTest : public ::testing::Test
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

TEST_F(ParamLoaderTest, LoadDoubleDeclaresDefaultWhenMissing)
{
    auto node = std::make_shared<rclcpp::Node>("param_test_double_default");
    EXPECT_DOUBLE_EQ(ParamLoader::load_double(node.get(), "missing", 1.25), 1.25);
    EXPECT_TRUE(node->has_parameter("missing"));
}

TEST_F(ParamLoaderTest, LoadDoubleReadsPresetValue)
{
    auto node = std::make_shared<rclcpp::Node>("param_test_double_preset");
    node->declare_parameter<double>("speed", 3.0);
    EXPECT_DOUBLE_EQ(ParamLoader::load_double(node.get(), "speed", 1.0), 3.0);
}

TEST_F(ParamLoaderTest, LoadDoubleValidatedClamps)
{
    auto node = std::make_shared<rclcpp::Node>("param_test_double_validated");
    node->declare_parameter<double>("high", 99.0);
    node->declare_parameter<double>("low", -5.0);
    EXPECT_DOUBLE_EQ(ParamLoader::load_double_validated(node.get(), "high", 1.0, 0.0, 10.0), 10.0);
    EXPECT_DOUBLE_EQ(ParamLoader::load_double_validated(node.get(), "low", 1.0, 0.0, 10.0), 0.0);
    EXPECT_DOUBLE_EQ(ParamLoader::load_double_validated(node.get(), "missing", 5.0, 0.0, 10.0), 5.0);
}

TEST_F(ParamLoaderTest, LoadBool)
{
    auto node = std::make_shared<rclcpp::Node>("param_test_bool");
    EXPECT_TRUE(ParamLoader::load_bool(node.get(), "flag", true));
    node->declare_parameter<bool>("other", false);
    EXPECT_FALSE(ParamLoader::load_bool(node.get(), "other", true));
}

TEST_F(ParamLoaderTest, LoadString)
{
    auto node = std::make_shared<rclcpp::Node>("param_test_string");
    EXPECT_EQ(ParamLoader::load_string(node.get(), "frame", "base_link"), "base_link");
    node->declare_parameter<std::string>("name", "lidar_link");
    EXPECT_EQ(ParamLoader::load_string(node.get(), "name", "base_link"), "lidar_link");
}

TEST_F(ParamLoaderTest, LoadIntValidatedClamps)
{
    auto node = std::make_shared<rclcpp::Node>("param_test_int");
    node->declare_parameter<int>("big", 1000);
    EXPECT_EQ(ParamLoader::load_int_validated(node.get(), "big", 5, 0, 100), 100);
    EXPECT_EQ(ParamLoader::load_int_validated(node.get(), "missing", 7, 0, 100), 7);
}
