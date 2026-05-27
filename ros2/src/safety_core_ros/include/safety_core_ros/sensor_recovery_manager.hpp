// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include "safety_core/result.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "safety_core_ros/time_utils.hpp"

#include <optional>
#include <rclcpp/logger.hpp>
#include <rclcpp/logging.hpp>
#include <safety_core_msgs/msg/sensor_health.hpp>
#include <string>
#include <unordered_map>

namespace safety_core_ros
{

    /**
     * @brief Manages per-sensor health tracking and degradation/recovery state transitions
     *
     * Encapsulates all sensor-recovery state that was previously scattered throughout
     * SafetySupervisorNode. The node remains responsible for the ROS callbacks and
     * diagnostics publishing; this struct owns the data and decision logic.
     */
    struct SensorRecoveryManager
    {
        // ---- Types ---------------------------------------------------------------

        struct SensorRecoveryInfo
        {
            uint8_t status{safety_core_msgs::msg::SensorHealth::UNKNOWN};
            std::optional<std::uint64_t> degradation_start_ns;
            bool timed_out{false};
        };

        // ---- Construction --------------------------------------------------------

        SensorRecoveryManager() = default;

        // ---- Public API ----------------------------------------------------------

        /**
         * @brief Update state from a sensor_health message.
         *
         * Returns true if the transition resulted in a new DEGRADED event (sensor went bad)
         * or a new RECOVERED event (sensor came back). The node uses the return value to
         * decide whether to call handle_sensor_degraded / handle_sensor_recovered.
         */
        enum class HealthTransition : uint8_t
        {
            kNone      = 0U,
            kDegraded  = 1U,
            kRecovered = 2U,
        };
        struct ProcessResult
        {
            HealthTransition transition{HealthTransition::kNone};
            std::string sensor_name;
        };

        ProcessResult process_health_msg(const std::string& name, uint8_t status, std::uint64_t now_ns);

        /**
         * @brief Check whether any degraded sensor has exceeded the recovery timeout.
         *
         * Returns the first timed-out sensor name if one is found, empty string otherwise.
         */
        std::string check_timeouts(std::uint64_t now_ns, std::uint64_t recovery_timeout_ns,
                                   const rclcpp::Logger& logger);

        /**
         * @brief Update worst_sensor_status_ and last_sensor_health_time_ns_ for
         * the "all" synthetic summary message.
         */
        void update_summary(uint8_t status, std::uint64_t now_ns);

        [[nodiscard]] bool is_any_degraded() const noexcept;
        [[nodiscard]] bool in_degraded_mode() const noexcept
        {
            return in_degraded_mode_;
        }
        void set_degraded_mode(bool val) noexcept
        {
            in_degraded_mode_ = val;
        }

        [[nodiscard]] uint8_t worst_status() const noexcept
        {
            return worst_sensor_status_;
        }
        [[nodiscard]] std::optional<std::uint64_t> last_health_time_ns() const noexcept
        {
            return last_sensor_health_time_ns_;
        }

        // ---- State ---------------------------------------------------------------

        std::unordered_map<std::string, SensorRecoveryInfo> sensor_states_;

      private:
        uint8_t worst_sensor_status_{safety_core_msgs::msg::SensorHealth::UNKNOWN};
        std::optional<std::uint64_t> last_sensor_health_time_ns_;
        bool in_degraded_mode_{false};
    };

    // ---- Inline implementations --------------------------------------------------

    inline SensorRecoveryManager::ProcessResult
    SensorRecoveryManager::process_health_msg(const std::string& name, uint8_t status, std::uint64_t now_ns)
    {
        auto it = sensor_states_.find(name);
        if (it == sensor_states_.end())
        {
            SensorRecoveryInfo info;
            info.status = status;
            if (status == safety_core_msgs::msg::SensorHealth::DEGRADED)
            {
                info.degradation_start_ns = now_ns;
            }
            sensor_states_.emplace(name, info);
            if (status == safety_core_msgs::msg::SensorHealth::DEGRADED)
            {
                return {HealthTransition::kDegraded, name};
            }
            return {HealthTransition::kNone, {}};
        }

        SensorRecoveryInfo& info = it->second;
        if (info.timed_out && status == safety_core_msgs::msg::SensorHealth::DEGRADED)
        {
            // Already timed out; ignore further degraded messages until sensor recovers
            return {HealthTransition::kNone, {}};
        }

        const uint8_t prev_status = info.status;
        if (prev_status == status)
        {
            return {HealthTransition::kNone, {}};
        }

        info.status = status;

        if (status == safety_core_msgs::msg::SensorHealth::DEGRADED)
        {
            info.degradation_start_ns = now_ns;
            return {HealthTransition::kDegraded, name};
        }
        else if (prev_status == safety_core_msgs::msg::SensorHealth::DEGRADED &&
                 status == safety_core_msgs::msg::SensorHealth::HEALTHY)
        {
            info.degradation_start_ns = std::nullopt;
            return {HealthTransition::kRecovered, name};
        }
        else if (status == safety_core_msgs::msg::SensorHealth::FAULT)
        {
            info.degradation_start_ns = std::nullopt;
        }
        return {HealthTransition::kNone, {}};
    }

    inline std::string SensorRecoveryManager::check_timeouts(std::uint64_t now_ns, std::uint64_t recovery_timeout_ns,
                                                             const rclcpp::Logger& logger)
    {
        for (auto& pair : sensor_states_)
        {
            SensorRecoveryInfo& info = pair.second;
            if (info.status != safety_core_msgs::msg::SensorHealth::DEGRADED || !info.degradation_start_ns.has_value())
            {
                continue;
            }

            const std::uint64_t elapsed_ns = now_ns - info.degradation_start_ns.value();
            if (elapsed_ns > recovery_timeout_ns)
            {
                RCLCPP_ERROR(logger, "Sensor '%s' recovery timeout exceeded (%.1fs) - treating as FAULT",
                             pair.first.c_str(), TimeUtils::nanoseconds_to_seconds(elapsed_ns));

                info.timed_out            = true;
                info.degradation_start_ns = std::nullopt;

                return pair.first;
            }
        }
        return {};
    }

    inline void SensorRecoveryManager::update_summary(uint8_t status, std::uint64_t now_ns)
    {
        worst_sensor_status_        = status;
        last_sensor_health_time_ns_ = now_ns;
    }

    inline bool SensorRecoveryManager::is_any_degraded() const noexcept
    {
        for (const auto& pair : sensor_states_)
        {
            if (pair.second.status == safety_core_msgs::msg::SensorHealth::DEGRADED)
            {
                return true;
            }
        }
        return false;
    }

} // namespace safety_core_ros
