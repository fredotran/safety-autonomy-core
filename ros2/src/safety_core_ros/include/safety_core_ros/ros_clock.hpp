#pragma once

#include "safety_core/platform/clock.hpp"

#include <rclcpp/clock.hpp>

namespace safety_core_ros
{

    // Adapter that exposes an rclcpp::Clock as a safety_core::platform::Clock so
    // the safety_core library sees a single coherent time source aligned with
    // /clock (sim time) when configured.
    class RosClock final : public safety_core::platform::Clock
    {
      public:
        explicit RosClock(rclcpp::Clock::SharedPtr clock) noexcept : clock_(std::move(clock)) {}

        [[nodiscard]] safety_core::time::TimePoint now() const noexcept override;

      private:
        rclcpp::Clock::SharedPtr clock_;
    };

} // namespace safety_core_ros
