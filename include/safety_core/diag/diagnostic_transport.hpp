#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace safety_core::diag
{

    constexpr std::size_t kDiagnosticTopicCapacity   = 64U;
    constexpr std::size_t kDiagnosticPayloadCapacity = 192U;

    struct DiagnosticEvent
    {
        std::array<char, kDiagnosticTopicCapacity> topic{};
        std::array<char, kDiagnosticPayloadCapacity> payload{};
        std::uint64_t timestamp_ns{0U};
        bool topic_truncated{false};
        bool payload_truncated{false};

        void set_topic(std::string_view value) noexcept
        {
            topic.fill('\0');
            topic_truncated            = value.size() >= topic.size();
            const std::size_t copy_len = (value.size() < (topic.size() - 1U)) ? value.size() : (topic.size() - 1U);
            for (std::size_t i = 0U; i < copy_len; ++i)
            {
                topic[i] = value[i];
            }
        }

        void set_payload(std::string_view value) noexcept
        {
            payload.fill('\0');
            payload_truncated          = value.size() >= payload.size();
            const std::size_t copy_len = (value.size() < (payload.size() - 1U)) ? value.size() : (payload.size() - 1U);
            for (std::size_t i = 0U; i < copy_len; ++i)
            {
                payload[i] = value[i];
            }
        }

        [[nodiscard]] std::string_view topic_view() const noexcept
        {
            return std::string_view(topic.data());
        }
        [[nodiscard]] std::string_view payload_view() const noexcept
        {
            return std::string_view(payload.data());
        }
    };

    class DiagnosticTransport
    {
      public:
        virtual ~DiagnosticTransport()                              = default;
        virtual void publish(const DiagnosticEvent& event) noexcept = 0;
    };

} // namespace safety_core::diag
