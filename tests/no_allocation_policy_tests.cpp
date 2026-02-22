#include "safety_core/config/system_config.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/platform/manual_clock.hpp"
#include "safety_core/system/context_factory.hpp"
#include "test_support.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

namespace
{
    std::atomic<std::size_t> g_allocation_count{0U};

    void* operator_new_impl(std::size_t size)
    {
        if (size == 0U)
        {
            size = 1U;
        }
        void* ptr = std::malloc(size);
        if (ptr == nullptr)
        {
            throw std::bad_alloc();
        }
        g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
        return ptr;
    }

    using safety_core::test_support::check;

    class CaptureTransport final : public safety_core::diag::DiagnosticTransport
    {
      public:
        void publish(const safety_core::diag::DiagnosticEvent& event) noexcept override
        {
            ++publish_count;
            last_event = event;
        }

        std::size_t publish_count{0U};
        safety_core::diag::DiagnosticEvent last_event{};
    };

    safety_core::config::SystemConfig make_defaults()
    {
        using namespace std::chrono_literals;

        safety_core::config::SystemConfig cfg{};
        cfg.config_version                  = safety_core::config::kCurrentSystemConfigVersion;
        cfg.timing.control_period           = 100ms;
        cfg.timing.watchdog_period          = 200ms;
        cfg.timing.localization_timeout     = 2s;
        cfg.envelope.max_speed_mps          = 1.2;
        cfg.envelope.max_accel_mps2         = 0.6;
        cfg.envelope.max_comfort_decel_mps2 = 0.8;
        cfg.envelope.control_latency_s      = 0.1;
        cfg.envelope.safety_buffer_m        = 0.2;
        cfg.max_tasks                       = 4U;
        return cfg;
    }

} // namespace

void* operator new(std::size_t size)
{
    return operator_new_impl(size);
}
void* operator new[](std::size_t size)
{
    return operator_new_impl(size);
}

void operator delete(void* ptr) noexcept
{
    std::free(ptr);
}
void operator delete[](void* ptr) noexcept
{
    std::free(ptr);
}
void operator delete(void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}
void operator delete[](void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return operator_new_impl(size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return operator_new_impl(size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
    std::free(ptr);
}
void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
    std::free(ptr);
}

int main()
{
    using safety_core::config::ValidationPolicy;
    using safety_core::system::ContextFactoryOptions;
    using safety_core::system::ContextWithConfig;

    bool ok = true;

    // Pre-seed env so build_context exercises migration + warning startup diagnostics without allocating.
    setenv("SAFETY_CORE_CONFIG_VERSION", "1", 1);
    setenv("SAFETY_CORE_SAFETY_BUFFER_M", "0", 1);

    safety_core::platform::ManualClock clock;
    CaptureTransport transport;

    ContextFactoryOptions options{};
    options.defaults             = make_defaults();
    options.validation_policy    = ValidationPolicy::AllowWarnings;
    options.clock                = &clock;
    options.diagnostic_transport = &transport;

    ContextWithConfig out{};
    const std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
    const auto result        = safety_core::system::build_context(out, options);
    const std::size_t after  = g_allocation_count.load(std::memory_order_relaxed);

    ok &= check(result.ok(), "build_context should succeed in allow-warnings mode");
    ok &= check(after == before, "build_context core path should not allocate from heap");
    ok &= check(transport.publish_count >= 2U, "expected migration and warning diagnostics to publish");

    unsetenv("SAFETY_CORE_CONFIG_VERSION");
    unsetenv("SAFETY_CORE_SAFETY_BUFFER_M");

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] no allocation policy tests" << '\n';
    return 0;
}
