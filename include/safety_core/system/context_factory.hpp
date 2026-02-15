#pragma once

#include "safety_core/config/system_config.hpp"
#include "safety_core/config/validation.hpp"
#include "safety_core/platform/clock.hpp"
#include "safety_core/result.hpp"
#include "safety_core/system/system_context.hpp"

namespace safety_core::system
{

    struct ContextFactoryOptions
    {
        config::SystemConfig defaults{};
        config::ValidationPolicy validation_policy{config::ValidationPolicy::Strict};
        platform::Clock* clock{nullptr};
        diag::HealthMonitor* health_monitor{nullptr};
        diag::DiagnosticTransport* diagnostic_transport{nullptr};
    };

    struct ContextWithConfig
    {
        config::SystemConfig config{};
        SystemContext context{};
    };

    Result build_context(ContextWithConfig& out,
                         const ContextFactoryOptions& options = ContextFactoryOptions{}) noexcept;

} // namespace safety_core::system
