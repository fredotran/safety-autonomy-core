#pragma once

#include "safety_core/diag/diagnostic_transport.hpp"

#include <atomic>
#include <memory>
#include <rclcpp/publisher.hpp>
#include <safety_core_msgs/msg/diagnostic_event.hpp>

namespace safety_core_ros
{

    // Adapts safety_core::diag::DiagnosticTransport so events emitted by the
    // library (state machine transitions, supervisor escalations, PID timeouts,
    // etc.) are republished as ROS 2 messages. We deliberately allow allocation
    // here since diagnostics are off the safety-critical hot path.
    class RosDiagnosticTransport final : public safety_core::diag::DiagnosticTransport
    {
      public:
        using PublisherT = rclcpp::Publisher<safety_core_msgs::msg::DiagnosticEvent>;

        explicit RosDiagnosticTransport(PublisherT::SharedPtr publisher) noexcept : publisher_(std::move(publisher)) {}

        void publish(const safety_core::diag::DiagnosticEvent& event) noexcept override;

        [[nodiscard]] std::uint64_t topic_truncation_count() const noexcept
        {
            return topic_truncation_count_.load(std::memory_order_relaxed);
        }
        [[nodiscard]] std::uint64_t payload_truncation_count() const noexcept
        {
            return payload_truncation_count_.load(std::memory_order_relaxed);
        }
        [[nodiscard]] std::uint64_t publish_failure_count() const noexcept
        {
            return publish_failure_count_.load(std::memory_order_relaxed);
        }

      private:
        PublisherT::SharedPtr publisher_;
        std::atomic<std::uint64_t> topic_truncation_count_{0U};
        std::atomic<std::uint64_t> payload_truncation_count_{0U};
        std::atomic<std::uint64_t> publish_failure_count_{0U};
    };

} // namespace safety_core_ros
