// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

// Guards the message contract that safety_core_ros / safety_core_nav2 code is
// compiled against: constant values must stay aligned with the safety_core
// C++ enums (sm::Mode, safety::SafetyZone) documented in the .msg files.

#include <gtest/gtest.h>
#include <safety_core_msgs/msg/envelope_status.hpp>
#include <safety_core_msgs/msg/safety_state.hpp>
#include <safety_core_msgs/msg/safety_zone.hpp>

using safety_core_msgs::msg::EnvelopeStatus;
using safety_core_msgs::msg::SafetyState;
using safety_core_msgs::msg::SafetyZone;

TEST(SafetyZoneMsg, ConstantValuesMirrorCoreEnum)
{
    // safety_core::safety::SafetyZone order is part of the wire contract:
    // safety_envelope_node publishes via static_cast<std::uint8_t>(zone).
    EXPECT_EQ(SafetyZone::CLEAR, 0U);
    EXPECT_EQ(SafetyZone::WARNING, 1U);
    EXPECT_EQ(SafetyZone::PROTECTIVE, 2U);
    EXPECT_EQ(SafetyZone::EMERGENCY, 3U);
}

TEST(SafetyStateMsg, ModeConstantsMirrorStateMachine)
{
    // Mirror of safety_core::sm::Mode ordering (Init..SafeStop).
    EXPECT_EQ(SafetyState::INIT, 0U);
    EXPECT_EQ(SafetyState::IDLE, 1U);
    EXPECT_EQ(SafetyState::MOVING, 2U);
    EXPECT_EQ(SafetyState::DEGRADED, 3U);
    EXPECT_EQ(SafetyState::AVOIDING_OBSTACLE, 4U);
    EXPECT_EQ(SafetyState::LOCALIZATION_LOST, 5U);
    EXPECT_EQ(SafetyState::DOCKING, 6U);
    EXPECT_EQ(SafetyState::SAFE_STOP, 7U);
}

TEST(SafetyStateMsg, FieldsRoundTrip)
{
    SafetyState msg;
    msg.mode                = SafetyState::MOVING;
    msg.fault_latched       = true;
    msg.fault_code          = 0x0042;
    msg.zone.zone           = SafetyZone::WARNING;
    msg.safe_stop_requested = false;
    msg.header.frame_id     = "base_link";

    EXPECT_EQ(msg.mode, 2U);
    EXPECT_TRUE(msg.fault_latched);
    EXPECT_EQ(msg.fault_code, 0x0042);
    EXPECT_EQ(msg.zone.zone, 1U);
    EXPECT_FALSE(msg.safe_stop_requested);
    EXPECT_EQ(msg.header.frame_id, "base_link");
}

TEST(EnvelopeStatusMsg, FieldsRoundTrip)
{
    EnvelopeStatus msg;
    msg.distance_to_obstacle_m      = 1.25;
    msg.current_speed_mps           = 0.5;
    msg.within_envelope             = true;
    msg.zone.zone                   = SafetyZone::CLEAR;
    msg.stopping_distance_m         = 0.9;
    msg.required_clearance_m        = 1.1;
    msg.recommended_speed_limit_mps = 1.5;
    msg.footprint_length_m          = 0.8;
    msg.footprint_width_m           = 0.6;
    msg.footprint_front_overhang_m  = 0.1;

    EXPECT_DOUBLE_EQ(msg.distance_to_obstacle_m, 1.25);
    EXPECT_DOUBLE_EQ(msg.required_clearance_m, 1.1);
    EXPECT_TRUE(msg.within_envelope);
}
