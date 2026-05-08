#pragma once

#include <rclcpp/node.hpp>
#include <string>

namespace safety_core_ros
{

    /**
     * @brief Utility class for loading and validating parameters
     *
     * Provides helper functions for loading parameters with default values
     * and validation, reducing code duplication and improving error handling.
     */
    class ParamLoader
    {
      public:
        /**
         * @brief Load a double parameter with default value
         * @param node ROS 2 node
         * @param param_name Parameter name
         * @param default_value Default value if parameter not set
         * @return Parameter value
         */
        static double load_double(rclcpp::Node* node, const std::string& param_name, double default_value)
        {
            // Try to get parameter without declaring first (in case it's already declared via YAML)
            if (node->has_parameter(param_name))
            {
                return node->get_parameter(param_name).as_double();
            }
            // If not found, declare with default
            node->declare_parameter<double>(param_name, default_value);
            return node->get_parameter(param_name).as_double();
        }

        /**
         * @brief Load a double parameter with validation
         * @param node ROS 2 node
         * @param param_name Parameter name
         * @param default_value Default value if parameter not set
         * @param min_value Minimum valid value
         * @param max_value Maximum valid value
         * @return Parameter value (clamped to valid range)
         */
        static double load_double_validated(rclcpp::Node* node, const std::string& param_name, double default_value,
                                            double min_value, double max_value)
        {
            double value = load_double(node, param_name, default_value);
            return std::clamp(value, min_value, max_value);
        }

        /**
         * @brief Load a boolean parameter with default value
         * @param node ROS 2 node
         * @param param_name Parameter name
         * @param default_value Default value if parameter not set
         * @return Parameter value
         */
        static bool load_bool(rclcpp::Node* node, const std::string& param_name, bool default_value)
        {
            // Try to get parameter without declaring first (in case it's already declared via YAML)
            if (node->has_parameter(param_name))
            {
                return node->get_parameter(param_name).as_bool();
            }
            // If not found, declare with default
            node->declare_parameter<bool>(param_name, default_value);
            return node->get_parameter(param_name).as_bool();
        }

        /**
         * @brief Load a string parameter with default value
         * @param node ROS 2 node
         * @param param_name Parameter name
         * @param default_value Default value if parameter not set
         * @return Parameter value
         */
        static std::string load_string(rclcpp::Node* node, const std::string& param_name,
                                       const std::string& default_value)
        {
            // Try to get parameter without declaring first (in case it's already declared via YAML)
            if (node->has_parameter(param_name))
            {
                return node->get_parameter(param_name).as_string();
            }
            // If not found, declare with default
            node->declare_parameter<std::string>(param_name, default_value);
            return node->get_parameter(param_name).as_string();
        }

        /**
         * @brief Load an integer parameter with default value
         * @param node ROS 2 node
         * @param param_name Parameter name
         * @param default_value Default value if parameter not set
         * @return Parameter value
         */
        static int load_int(rclcpp::Node* node, const std::string& param_name, int default_value)
        {
            // Try to get parameter without declaring first (in case it's already declared via YAML)
            if (node->has_parameter(param_name))
            {
                return node->get_parameter(param_name).as_int();
            }
            // If not found, declare with default
            node->declare_parameter<int>(param_name, default_value);
            return node->get_parameter(param_name).as_int();
        }

        /**
         * @brief Load an integer parameter with validation
         * @param node ROS 2 node
         * @param param_name Parameter name
         * @param default_value Default value if parameter not set
         * @param min_value Minimum valid value
         * @param max_value Maximum valid value
         * @return Parameter value (clamped to valid range)
         */
        static int load_int_validated(rclcpp::Node* node, const std::string& param_name, int default_value,
                                      int min_value, int max_value)
        {
            int value = load_int(node, param_name, default_value);
            return std::clamp(value, min_value, max_value);
        }
    };

} // namespace safety_core_ros
