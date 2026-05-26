// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#pragma once

#include <cmath>

namespace safety_core_test
{

    /**
     * @brief Compare two doubles for approximate equality
     */
    inline bool approx_equal(double a, double b, double epsilon = 1e-9)
    {
        return std::fabs(a - b) < epsilon;
    }

} // namespace safety_core_test
