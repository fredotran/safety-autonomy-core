// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/system/context_factory.hpp"

#include "safety_core/config/env_loader.hpp"
#include "safety_core/config/migration.hpp"
#include "safety_core/config/validation.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/topics.hpp"

#include <chrono>
#include <cstdio>
#include <string_view>

namespace safety_core::system
{

    namespace
    {
        static_assert(96U < diag::kDiagnosticPayloadCapacity);

        void publish_startup_event(diag::DiagnosticTransport* transport, std::string_view topic,
                                   std::string_view payload, const platform::Clock* clock) noexcept
        {
            if (transport == nullptr)
            {
                return;
            }

            const time::TimePoint stamp = (clock != nullptr) ? clock->now() : time::now();

            diag::DiagnosticEvent event{};
            event.set_topic(topic);
            event.set_payload(payload);
            event.timestamp_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(stamp.time_since_epoch()).count());
            transport->publish(event);
        }
    } // namespace

    Result build_context(ContextWithConfig& out, const ContextFactoryOptions& options) noexcept
    {
        const auto loaded   = config::load_from_env(options.defaults);
        const auto migrated = config::migrate_to_current(loaded);
        if (!migrated.ok)
        {
            return Result::Fault(migrated.message);
        }

        if (migrated.migrated)
        {
            char payload[96]{};
            static_cast<void>(std::snprintf(payload, sizeof(payload), "%.*s from=%u to=%u",
                                            static_cast<int>(migrated.message.size()), migrated.message.data(),
                                            static_cast<unsigned>(migrated.from_version),
                                            static_cast<unsigned>(migrated.to_version)));
            publish_startup_event(options.diagnostic_transport, diag::topic::kConfigMigration,
                                  std::string_view(payload), options.clock);
        }

        const auto validation = config::validate(migrated.config, options.validation_policy);
        if (!validation.ok)
        {
            return Result::Fault(validation.message);
        }

        if (validation.warning)
        {
            publish_startup_event(options.diagnostic_transport, diag::topic::kConfigValidationWarning,
                                  validation.message, options.clock);
        }

        out.config                 = migrated.config;
        out.context.config         = &out.config;
        out.context.clock          = (options.clock != nullptr) ? options.clock : platform::SteadyClock::instance_ptr();
        out.context.health_monitor = options.health_monitor;
        out.context.diagnostic_transport = options.diagnostic_transport;
        out.context.safety_supervisor    = options.safety_supervisor;
        return Result::Ok();
    }

} // namespace safety_core::system
