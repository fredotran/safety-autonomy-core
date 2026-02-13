#include "safety_core/platform/clock.hpp"

namespace safety_core::platform
{

    SteadyClock& SteadyClock::instance() noexcept
    {
        static SteadyClock clock;
        return clock;
    }

    Clock* SteadyClock::instance_ptr() noexcept
    {
        return &instance();
    }

    time::TimePoint SteadyClock::now() const noexcept
    {
        return time::now();
    }

} // namespace safety_core::platform
