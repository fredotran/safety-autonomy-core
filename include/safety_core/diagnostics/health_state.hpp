#pragma once

#include <cstdint>

namespace safety_core::diag
{

    enum class HealthStatus : std::uint8_t
    {
        kUnknown = 0U,
        kHealthy,
        kDegraded,
        kFault,
    };

    struct HealthState
    {
        HealthStatus status{HealthStatus::kUnknown};
        std::uint16_t fault_code{0U};
        bool latched{false};

        [[nodiscard]] constexpr bool ok() const noexcept
        {
            return status == HealthStatus::kHealthy;
        }
        [[nodiscard]] constexpr bool faulted() const noexcept
        {
            return status == HealthStatus::kFault;
        }
    };

} // namespace safety_core::diag
