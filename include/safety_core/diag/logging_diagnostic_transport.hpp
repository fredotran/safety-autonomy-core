#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/diag/diagnostic_transport.hpp"

#include <iostream>
#include <ostream>

namespace safety_core::diag
{

    class LoggingDiagnosticTransport final : public DiagnosticTransport
    {
      public:
        explicit LoggingDiagnosticTransport(std::ostream& sink = std::cout) noexcept : sink_(sink) {}

        void publish(const DiagnosticEvent& event) noexcept override;

        [[nodiscard]] std::uint64_t topic_truncation_count() const noexcept
        {
            return topic_truncation_count_;
        }
        [[nodiscard]] std::uint64_t payload_truncation_count() const noexcept
        {
            return payload_truncation_count_;
        }

        void reset_counters() noexcept
        {
            topic_truncation_count_   = 0U;
            payload_truncation_count_ = 0U;
        }

      private:
        std::ostream& sink_;
        std::uint64_t topic_truncation_count_{0U};
        std::uint64_t payload_truncation_count_{0U};
    };

} // namespace safety_core::diag
