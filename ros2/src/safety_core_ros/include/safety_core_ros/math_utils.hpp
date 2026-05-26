// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace safety_core_ros
{

    /**
     * @brief Utility class for mathematical operations
     *
     * Provides helper functions for common mathematical operations used
     * throughout the safety system, particularly for geometric calculations
     * and optimizations.
     */
    class MathUtils
    {
      public:
        /**
         * @brief Check if a value is within a valid range
         * @param value Value to check
         * @param min Minimum valid value
         * @param max Maximum valid value
         * @return true if value is valid, false otherwise
         */
        static constexpr bool is_in_range(double value, double min, double max) noexcept
        {
            return value >= min && value <= max;
        }

        /**
         * @brief Clamp a value to a range
         * @param value Value to clamp
         * @param min Minimum value
         * @param max Maximum value
         * @return Clamped value
         */
        static constexpr double clamp(double value, double min, double max) noexcept
        {
            return std::clamp(value, min, max);
        }

        /**
         * @brief Check if a value is finite and valid
         * @param value Value to check
         * @return true if value is finite, false otherwise
         */
        static constexpr bool is_finite(double value) noexcept
        {
            return std::isfinite(value);
        }

        /**
         * @brief Get the minimum of two values
         * @param a First value
         * @param b Second value
         * @return Minimum value
         */
        static constexpr double min(double a, double b) noexcept
        {
            return std::min(a, b);
        }

        /**
         * @brief Get the maximum of two values
         * @param a First value
         * @param b Second value
         * @return Maximum value
         */
        static constexpr double max(double a, double b) noexcept
        {
            return std::max(a, b);
        }

        /**
         * @brief Calculate squared distance (avoids sqrt for performance)
         * @param x X coordinate
         * @param y Y coordinate
         * @return Squared distance from origin
         */
        static constexpr double squared_distance(double x, double y) noexcept
        {
            return x * x + y * y;
        }

        /**
         * @brief Check if a point is within a corridor width
         * @param y Lateral coordinate
         * @param corridor_half_width Half-width of the corridor
         * @return true if point is within corridor, false otherwise
         */
        static constexpr bool is_in_corridor(double y, double corridor_half_width) noexcept
        {
            return std::fabs(y) <= corridor_half_width;
        }

        /**
         * @brief Check if a point is within a corridor using squared comparison
         * @param y Lateral coordinate
         * @param corridor_half_width_sq Squared half-width of the corridor
         * @return true if point is within corridor, false otherwise
         */
        static constexpr bool is_in_corridor_squared(double y, double corridor_half_width_sq) noexcept
        {
            return y * y <= corridor_half_width_sq;
        }

        /**
         * @brief Convert degrees to radians
         * @param degrees Angle in degrees
         * @return Angle in radians
         */
        static constexpr double degrees_to_radians(double degrees) noexcept
        {
            return degrees * M_PI / 180.0;
        }

        /**
         * @brief Convert radians to degrees
         * @param radians Angle in radians
         * @return Angle in degrees
         */
        static constexpr double radians_to_degrees(double radians) noexcept
        {
            return radians * 180.0 / M_PI;
        }

        /**
         * @brief Linear interpolation between two values
         * @param a Start value
         * @param b End value
         * @param t Interpolation factor (0.0 to 1.0)
         * @return Interpolated value
         */
        static constexpr double lerp(double a, double b, double t) noexcept
        {
            return a + t * (b - a);
        }
    };

    /**
     * @brief Math utility free functions namespace
     */
    namespace math
    {
        /**
         * @brief Clamp a value to a range
         * @param value Value to clamp
         * @param min Minimum value
         * @param max Maximum value
         * @return Clamped value
         */
        inline double clamp(double value, double min, double max) noexcept
        {
            return std::clamp(value, min, max);
        }

        /**
         * @brief Apply a deadband to a value
         * @param value Input value
         * @param band Deadband half-width
         * @return 0.0 if |value| <= band, otherwise value
         */
        inline double deadband(double value, double band) noexcept
        {
            return (std::fabs(value) <= band) ? 0.0 : value;
        }

        /**
         * @brief Calculate exponential decay
         * @param initial Initial value
         * @param dt Time elapsed
         * @param tau Time constant
         * @return Decayed value: initial * exp(-dt / tau)
         */
        inline double exponential_decay(double initial, double dt, double tau) noexcept
        {
            if (tau <= 0.0)
            {
                return initial;
            }
            return initial * std::exp(-dt / tau);
        }
    } // namespace math

} // namespace safety_core_ros
