#include "safety_core/diag/health_beacon_publisher.hpp"
#include "safety_core/diag/topics.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "test_support.hpp"

#include <chrono>
#include <iostream>
#include <string_view>

namespace
{
    using safety_core::test_support::check;
    using CaptureTransport = safety_core::test_support::CaptureTransport;

} // namespace

int main()
{
    using safety_core::diag::HealthBeaconPublisher;
    using safety_core::sm::Mode;
    using safety_core::time::Duration;
    using safety_core::time::TimePoint;

    bool ok = true;

    CaptureTransport transport;
    HealthBeaconPublisher publisher(&transport);
    publisher.set_period(std::chrono::milliseconds(100));

    const auto t0 = TimePoint(Duration(0));
    ok &= check(publisher.publish_if_due(t0, Mode::Moving, false, 50U, 1U, 2U), "first beacon publish should succeed");
    ok &= check(!publisher.publish_if_due(t0 + std::chrono::milliseconds(50), Mode::Moving, false, 30U, 1U, 2U),
                "beacon should not publish before period elapses");
    ok &= check(publisher.publish_if_due(t0 + std::chrono::milliseconds(120), Mode::SafeStop, true, 90U, 3U, 4U),
                "beacon should publish once period elapses");

    ok &= check(transport.events.size() == 2U, "expected exactly two beacon events");
    if (transport.events.size() == 2U)
    {
        ok &= check(transport.events[0].topic_view() == safety_core::diag::topic::kHealthBeacon,
                    "beacon topic should match typed constant");
        ok &= check(transport.events[1].payload_view().find("fault=1") != std::string_view::npos,
                    "beacon payload should include fault latch flag");
    }

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] health beacon tests" << '\n';
    return 0;
}
