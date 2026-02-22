#pragma once

#include "safety_core/result.hpp"

#include <cstdint>
#include <string_view>

namespace safety_core::diag
{
    class DiagnosticTransport;
}

namespace safety_core::platform
{
    class Clock;
}

namespace safety_core::sm
{
    class ModeStateMachine;
}

namespace safety_core::safety
{

    enum class MonitorSeverity : std::uint8_t
    {
        None = 0U,
        Warning,
        Degraded,
        Critical,
    };

    enum class MonitorType : std::uint8_t
    {
        ClockRegression = 0U,
        LocalizationStale,
        TrajectoryEnvelope,
        DiagnosticDrop,
    };

    struct MonitorEvent
    {
        MonitorType type{MonitorType::ClockRegression};
        MonitorSeverity severity{MonitorSeverity::None};
        std::uint16_t code{0U};
        std::string_view detail{};
    };

    class SafetySupervisor
    {
      public:
        explicit SafetySupervisor(sm::ModeStateMachine* machine        = nullptr,
                                  diag::DiagnosticTransport* transport = nullptr,
                                  platform::Clock* clock               = nullptr) noexcept;

        void set_state_machine(sm::ModeStateMachine* machine) noexcept;
        void set_transport(diag::DiagnosticTransport* transport) noexcept;
        void set_clock(platform::Clock* clock) noexcept;

        Result process_monitor_event(const MonitorEvent& event) noexcept;

        Result observe_clock_sample(std::uint64_t now_ns) noexcept;
        Result observe_localization_age(std::uint64_t localization_age_ns,
                                        std::uint64_t localization_timeout_ns) noexcept;
        Result observe_transport_drop_count(std::uint64_t dropped_since_last_sample, std::uint64_t warning_threshold,
                                            std::uint64_t critical_threshold) noexcept;

      private:
        Result escalate_warning(const MonitorEvent& event) noexcept;
        Result escalate_degraded(const MonitorEvent& event) noexcept;
        Result escalate_critical(const MonitorEvent& event) noexcept;

        void publish_monitor_event(std::string_view topic, const MonitorEvent& event) const noexcept;

        sm::ModeStateMachine* machine_{nullptr};
        diag::DiagnosticTransport* transport_{nullptr};
        platform::Clock* clock_{nullptr};

        bool has_clock_sample_{false};
        std::uint64_t last_clock_ns_{0U};
    };

} // namespace safety_core::safety
