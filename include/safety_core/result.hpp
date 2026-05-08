#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <string_view>

namespace safety_core
{

    enum class StatusCode : std::uint8_t
    {
        kOk           = 0U,
        kInvalidState = 1U,
        kDeadlineMiss = 2U,
        kFault        = 3U,
    };

    struct Result
    {
        StatusCode code{StatusCode::kOk};
        std::string_view message{};

        constexpr bool ok() const noexcept
        {
            return code == StatusCode::kOk;
        }

        static constexpr Result Ok() noexcept
        {
            return Result{};
        }
        static constexpr Result Fault(std::string_view msg) noexcept
        {
            return Result{StatusCode::kFault, msg};
        }
        static constexpr Result InvalidState(std::string_view msg) noexcept
        {
            return Result{StatusCode::kInvalidState, msg};
        }
        static constexpr Result DeadlineMiss(std::string_view msg) noexcept
        {
            return Result{StatusCode::kDeadlineMiss, msg};
        }
    };

} // namespace safety_core
