#include "safety_core/diag/logging_health_monitor.hpp"

namespace safety_core::diag
{

    LoggingHealthMonitor::LoggingHealthMonitor(std::ostream& sink) noexcept : sink_(&sink) {}

    void LoggingHealthMonitor::on_mode_transition(sm::Mode from, sm::Mode to) noexcept
    {
        if (sink_ == nullptr)
        {
            return;
        }
        (*sink_) << "mode_transition from=" << static_cast<int>(from) << " to=" << static_cast<int>(to) << '\n';
    }

    void LoggingHealthMonitor::on_fault_latched(std::uint16_t fault_code) noexcept
    {
        if (sink_ == nullptr)
        {
            return;
        }
        (*sink_) << "fault_latched code=" << fault_code << '\n';
    }

} // namespace safety_core::diag
