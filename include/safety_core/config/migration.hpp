#pragma once

#include "safety_core/config/system_config.hpp"

#include <string_view>

namespace safety_core::config
{

    struct MigrationResult
    {
        bool ok{true};
        bool migrated{false};
        SystemConfig config{};
        std::uint16_t from_version{kCurrentSystemConfigVersion};
        std::uint16_t to_version{kCurrentSystemConfigVersion};
        std::string_view message{};

        [[nodiscard]] static constexpr MigrationResult
        Success(const SystemConfig& cfg, bool did_migrate = false, std::string_view msg = {},
                std::uint16_t from_version = kCurrentSystemConfigVersion,
                std::uint16_t to_version   = kCurrentSystemConfigVersion) noexcept
        {
            return MigrationResult{true, did_migrate, cfg, from_version, to_version, msg};
        }

        [[nodiscard]] static constexpr MigrationResult
        Error(std::string_view msg, const SystemConfig& cfg = {},
              std::uint16_t from_version = kCurrentSystemConfigVersion,
              std::uint16_t to_version   = kCurrentSystemConfigVersion) noexcept
        {
            return MigrationResult{false, false, cfg, from_version, to_version, msg};
        }
    };

    MigrationResult migrate_to_current(const SystemConfig& input) noexcept;

} // namespace safety_core::config
