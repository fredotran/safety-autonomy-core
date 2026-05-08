// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/diag/logging_diagnostic_transport.hpp"

namespace safety_core::diag
{

    void LoggingDiagnosticTransport::publish(const DiagnosticEvent& event) noexcept
    {
        if (event.topic_truncated)
        {
            ++topic_truncation_count_;
        }
        if (event.payload_truncated)
        {
            ++payload_truncation_count_;
        }

        sink_ << "[diag] topic=" << event.topic_view() << " payload=" << event.payload_view()
              << " ts=" << event.timestamp_ns << " topic_truncated=" << (event.topic_truncated ? 1 : 0)
              << " payload_truncated=" << (event.payload_truncated ? 1 : 0)
              << " topic_trunc_total=" << topic_truncation_count_
              << " payload_trunc_total=" << payload_truncation_count_ << '\n';
    }

} // namespace safety_core::diag
