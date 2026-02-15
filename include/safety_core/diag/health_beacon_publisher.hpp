#pragma once

#include "safety_core/common/time.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/topics.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace safety_core::sm
{
    enum class Mode : std::uint8_t;
}

namespace safety_core::diag
{

    // Commentary: Periodic beacon publisher centralizes observability without requiring heap allocations.
    class HealthBeaconPublisher
    {
      public:
        explicit HealthBeaconPublisher(DiagnosticTransport* transport = nullptr) noexcept : transport_(transport) {}

        void set_transport(DiagnosticTransport* transport) noexcept
        {
            transport_ = transport;
        }

        void set_period(time::Duration period) noexcept
        {
            period_ = (period.count() <= 0) ? time::Duration::zero() : period;
        }

        bool publish_if_due(time::TimePoint now_tp, sm::Mode mode, bool fault_latched, std::uint64_t watchdog_lag_ns,
                            std::uint64_t topic_truncations, std::uint64_t payload_truncations) noexcept
        {
            if ((transport_ == nullptr) || (period_.count() <= 0))
            {
                return false;
            }
            if (has_published_ && ((now_tp - last_publish_) < period_))
            {
                return false;
            }

            std::array<char, 160> payload{};
            static_cast<void>(std::snprintf(payload.data(), payload.size(),
                                            "mode=%u fault=%u watchdog_lag_ns=%llu topic_trunc=%llu payload_trunc=%llu",
                                            static_cast<unsigned>(mode), static_cast<unsigned>(fault_latched ? 1U : 0U),
                                            static_cast<unsigned long long>(watchdog_lag_ns),
                                            static_cast<unsigned long long>(topic_truncations),
                                            static_cast<unsigned long long>(payload_truncations)));

            DiagnosticEvent event{};
            event.set_topic(topic::kHealthBeacon);
            event.set_payload(std::string_view(payload.data()));
            event.timestamp_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now_tp.time_since_epoch()).count());
            transport_->publish(event);

            last_publish_  = now_tp;
            has_published_ = true;
            return true;
        }

      private:
        DiagnosticTransport* transport_{nullptr};
        time::Duration period_{time::Duration::zero()};
        time::TimePoint last_publish_{};
        bool has_published_{false};
    };

} // namespace safety_core::diag
