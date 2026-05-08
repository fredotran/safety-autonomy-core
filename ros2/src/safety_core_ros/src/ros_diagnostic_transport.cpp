#include "safety_core_ros/ros_diagnostic_transport.hpp"

#include <rclcpp/rclcpp.hpp>

namespace safety_core_ros
{

    void RosDiagnosticTransport::publish(const safety_core::diag::DiagnosticEvent& event) noexcept
    {
        if (!publisher_)
        {
            publish_failure_count_.fetch_add(1U, std::memory_order_relaxed);
            return;
        }

        if (event.topic_truncated)
        {
            topic_truncation_count_.fetch_add(1U, std::memory_order_relaxed);
        }
        if (event.payload_truncated)
        {
            payload_truncation_count_.fetch_add(1U, std::memory_order_relaxed);
        }

        try
        {
            safety_core_msgs::msg::DiagnosticEvent msg;
            msg.header.stamp.sec     = static_cast<std::int32_t>(event.timestamp_ns / 1'000'000'000ULL);
            msg.header.stamp.nanosec = static_cast<std::uint32_t>(event.timestamp_ns % 1'000'000'000ULL);
            msg.topic                = std::string(event.topic_view());
            msg.payload              = std::string(event.payload_view());
            msg.topic_truncated      = event.topic_truncated;
            msg.payload_truncated    = event.payload_truncated;
            msg.source_timestamp_ns  = event.timestamp_ns;
            publisher_->publish(std::move(msg));
        }
        catch (const std::exception&)
        {
            publish_failure_count_.fetch_add(1U, std::memory_order_relaxed);
        }
        catch (...)
        {
            publish_failure_count_.fetch_add(1U, std::memory_order_relaxed);
        }
    }

} // namespace safety_core_ros
