#pragma once

#include <rclcpp/qos.hpp>

namespace safety_core_ros
{

    /**
     * @brief Utility class for creating standardized QoS profiles
     *
     * This class provides factory methods for creating common QoS profiles
     * used throughout the safety system, ensuring consistency and reducing
     * code duplication.
     */
    class QosConfig
    {
      public:
        /**
         * @brief Create QoS profile for sensor data (best-effort, low latency)
         * @param history_depth Number of messages to keep in history (default: 5)
         * @return Configured QoS profile for sensor data
         */
        static rclcpp::QoS sensor_qos(std::size_t history_depth = 5)
        {
            rclcpp::QoS qos(history_depth);
            qos.best_effort();
            qos.durability_volatile();
            return qos;
        }

        /**
         * @brief Create QoS profile for reliable communication
         * @param history_depth Number of messages to keep in history (default: 10)
         * @return Configured QoS profile for reliable communication
         */
        static rclcpp::QoS reliable_qos(std::size_t history_depth = 10)
        {
            rclcpp::QoS qos(history_depth);
            qos.reliable();
            qos.durability_volatile();
            return qos;
        }

        /**
         * @brief Create QoS profile for state publishing (reliable, volatile)
         * @param history_depth Number of messages to keep in history (default: 10)
         * @return Configured QoS profile for state publishing
         */
        static rclcpp::QoS state_qos(std::size_t history_depth = 10)
        {
            return reliable_qos(history_depth);
        }

        /**
         * @brief Create QoS profile for actuator commands (reliable, low latency)
         * @param history_depth Number of messages to keep in history (default: 10)
         * @return Configured QoS profile for actuator commands
         */
        static rclcpp::QoS actuator_qos(std::size_t history_depth = 10)
        {
            return reliable_qos(history_depth);
        }
    };

} // namespace safety_core_ros
