#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/config/system_config.hpp"

namespace safety_core::config
{

    SystemConfig load_from_env(SystemConfig defaults = SystemConfig{}) noexcept;

} // namespace safety_core::config
