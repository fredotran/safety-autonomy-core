#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/logging_diagnostic_transport.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace
{

    bool check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "[FAIL] " << message << '\n';
            return false;
        }
        return true;
    }

} // namespace

int main()
{
    using safety_core::diag::DiagnosticEvent;
    using safety_core::diag::kDiagnosticPayloadCapacity;
    using safety_core::diag::kDiagnosticTopicCapacity;
    using safety_core::diag::LoggingDiagnosticTransport;

    bool ok = true;

    DiagnosticEvent event{};
    event.set_topic(std::string(kDiagnosticTopicCapacity + 8U, 'T'));
    event.set_payload(std::string(kDiagnosticPayloadCapacity + 16U, 'P'));

    ok &= check(event.topic_truncated, "topic truncation flag should be set");
    ok &= check(event.payload_truncated, "payload truncation flag should be set");
    ok &= check(event.topic_view().size() == (kDiagnosticTopicCapacity - 1U), "topic length should be clamped");
    ok &= check(event.payload_view().size() == (kDiagnosticPayloadCapacity - 1U), "payload length should be clamped");
    ok &= check(event.topic[kDiagnosticTopicCapacity - 1U] == '\0', "topic buffer must remain null-terminated");
    ok &= check(event.payload[kDiagnosticPayloadCapacity - 1U] == '\0', "payload buffer must remain null-terminated");

    std::ostringstream sink;
    LoggingDiagnosticTransport transport(sink);
    event.timestamp_ns = 123U;
    transport.publish(event);

    const std::string line = sink.str();
    ok &= check(line.find("topic_truncated=1") != std::string::npos, "log should include topic truncation marker");
    ok &= check(line.find("payload_truncated=1") != std::string::npos, "log should include payload truncation marker");
    ok &= check(transport.topic_truncation_count() == 1U, "topic truncation counter should increment");
    ok &= check(transport.payload_truncation_count() == 1U, "payload truncation counter should increment");

    DiagnosticEvent non_truncated{};
    non_truncated.set_topic("short.topic");
    non_truncated.set_payload("short-payload");
    ok &= check(!non_truncated.topic_truncated, "short topic should not be truncated");
    ok &= check(!non_truncated.payload_truncated, "short payload should not be truncated");
    transport.publish(non_truncated);
    ok &= check(transport.topic_truncation_count() == 1U,
                "topic truncation counter should not change on non-truncated event");
    ok &= check(transport.payload_truncation_count() == 1U,
                "payload truncation counter should not change on non-truncated event");

    transport.reset_counters();
    ok &= check(transport.topic_truncation_count() == 0U, "topic truncation counter reset should clear state");
    ok &= check(transport.payload_truncation_count() == 0U, "payload truncation counter reset should clear state");

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] diagnostic truncation tests" << '\n';
    return 0;
}
