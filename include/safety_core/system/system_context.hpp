#pragma once

#include "safety_core/config/system_config.hpp"
#include "safety_core/diag/health_monitor.hpp"
#include "safety_core/platform/clock.hpp"

namespace safety_core::system
{

    struct SystemContext
    {
        const config::SystemConfig* config{nullptr};
        platform::Clock* clock{nullptr};
        diag::HealthMonitor* health_monitor{nullptr};
    };

} // namespace safety_core::system
