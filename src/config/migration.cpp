#include "safety_core/config/migration.hpp"

namespace safety_core::config
{

    MigrationResult migrate_to_current(const SystemConfig& input) noexcept
    {
        if (input.config_version == kCurrentSystemConfigVersion)
        {
            return MigrationResult::Success(input, false, {}, input.config_version, input.config_version);
        }

        if (input.config_version == kLegacySystemConfigVersion)
        {
            SystemConfig upgraded   = input;
            upgraded.config_version = kCurrentSystemConfigVersion;
            return MigrationResult::Success(upgraded, true, "migrated config v1 to v2", input.config_version,
                                            kCurrentSystemConfigVersion);
        }

        if (input.config_version == 0U)
        {
            SystemConfig upgraded   = input;
            upgraded.config_version = kCurrentSystemConfigVersion;
            return MigrationResult::Success(upgraded, true, "migrated unversioned config to v2", input.config_version,
                                            kCurrentSystemConfigVersion);
        }

        return MigrationResult::Error("unsupported future config version", input, input.config_version,
                                      kCurrentSystemConfigVersion);
    }

} // namespace safety_core::config
