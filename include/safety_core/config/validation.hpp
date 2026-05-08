#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/config/system_config.hpp"

#include <string_view>

namespace safety_core::config
{

    enum class ValidationPolicy : std::uint8_t
    {
        Strict = 0U,
        AllowWarnings,
    };

    struct ValidationResult
    {
        bool ok{true};
        bool warning{false};
        std::string_view message{};

        [[nodiscard]] static constexpr ValidationResult Success() noexcept
        {
            return ValidationResult{};
        }
        [[nodiscard]] static constexpr ValidationResult Warning(std::string_view msg) noexcept
        {
            return ValidationResult{true, true, msg};
        }
        [[nodiscard]] static constexpr ValidationResult Error(std::string_view msg) noexcept
        {
            return ValidationResult{false, false, msg};
        }
    };

    ValidationResult validate(const SystemConfig& cfg, ValidationPolicy policy = ValidationPolicy::Strict) noexcept;

} // namespace safety_core::config
