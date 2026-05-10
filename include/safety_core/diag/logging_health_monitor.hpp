#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/diag/health_monitor.hpp"
#include "safety_core/platform/clock.hpp"

#include <ostream>

namespace safety_core::diag
{

    class LoggingHealthMonitor final : public HealthMonitor
    {
      public:
        explicit LoggingHealthMonitor(std::ostream& sink) noexcept;
        LoggingHealthMonitor(std::ostream& sink, platform::Clock* clock) noexcept;

        void on_mode_transition(sm::Mode from, sm::Mode to) noexcept override;
        void on_fault_latched(std::uint16_t fault_code) noexcept override;

        void set_sink(std::ostream& sink) noexcept
        {
            sink_ = &sink;
        }

        void set_clock(platform::Clock* clock) noexcept
        {
            clock_ = clock;
        }

      private:
        std::ostream* sink_;
        platform::Clock* clock_{nullptr};
    };

} // namespace safety_core::diag
