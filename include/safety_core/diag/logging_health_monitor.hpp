#pragma once

#include "safety_core/diag/health_monitor.hpp"

#include <ostream>

namespace safety_core::diag
{

    class LoggingHealthMonitor final : public HealthMonitor
    {
      public:
        explicit LoggingHealthMonitor(std::ostream& sink) noexcept;

        void on_mode_transition(sm::Mode from, sm::Mode to) noexcept override;
        void on_fault_latched(std::uint16_t fault_code) noexcept override;

        void set_sink(std::ostream& sink) noexcept
        {
            sink_ = &sink;
        }

      private:
        std::ostream* sink_;
    };

} // namespace safety_core::diag
