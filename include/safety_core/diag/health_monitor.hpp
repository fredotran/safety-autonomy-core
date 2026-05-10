#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include <cstdint>

namespace safety_core::sm
{
    enum class Mode : std::uint8_t;
}

namespace safety_core::diag
{

    class HealthMonitor
    {
      public:
        virtual ~HealthMonitor()                                             = default;
        virtual void on_mode_transition(sm::Mode from, sm::Mode to) noexcept = 0;
        virtual void on_fault_latched(std::uint16_t fault_code) noexcept     = 0;
    };

} // namespace safety_core::diag
