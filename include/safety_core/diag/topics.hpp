#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/diag/diagnostic_transport.hpp"

#include <string_view>

namespace safety_core::diag::topic
{

    constexpr std::string_view kModeTransition = "mode_transition";
    constexpr std::string_view kFaultLatched   = "fault_latched";

    constexpr std::string_view kPidLocalizationStale = "pid.localization_stale";
    constexpr std::string_view kPidOutput            = "pid.output";

    constexpr std::string_view kExecutorTaskAdded        = "executor.task_added";
    constexpr std::string_view kExecutorTaskRun          = "executor.task_run";
    constexpr std::string_view kExecutorTaskError        = "executor.task_error";
    constexpr std::string_view kExecutorDeadlineMiss     = "executor.deadline_miss";
    constexpr std::string_view kExecutorWatchdogExceeded = "executor.watchdog_exceeded";
    constexpr std::string_view kExecutorCatchUpLimited   = "executor.catch_up_limited";

    constexpr std::string_view kConfigMigration         = "config.migration";
    constexpr std::string_view kConfigValidationWarning = "config.validation_warning";
    constexpr std::string_view kHealthBeacon            = "health.beacon";
    constexpr std::string_view kSafetyMonitorWarning    = "safety.monitor_warning";
    constexpr std::string_view kSafetyDegradedRequest   = "safety.degraded_request";
    constexpr std::string_view kSafetySafeStopForced    = "safety.safestop_forced";

    static_assert(kModeTransition.size() < kDiagnosticTopicCapacity);
    static_assert(kFaultLatched.size() < kDiagnosticTopicCapacity);
    static_assert(kPidLocalizationStale.size() < kDiagnosticTopicCapacity);
    static_assert(kPidOutput.size() < kDiagnosticTopicCapacity);
    static_assert(kExecutorTaskAdded.size() < kDiagnosticTopicCapacity);
    static_assert(kExecutorTaskRun.size() < kDiagnosticTopicCapacity);
    static_assert(kExecutorTaskError.size() < kDiagnosticTopicCapacity);
    static_assert(kExecutorDeadlineMiss.size() < kDiagnosticTopicCapacity);
    static_assert(kExecutorWatchdogExceeded.size() < kDiagnosticTopicCapacity);
    static_assert(kExecutorCatchUpLimited.size() < kDiagnosticTopicCapacity);
    static_assert(kConfigMigration.size() < kDiagnosticTopicCapacity);
    static_assert(kConfigValidationWarning.size() < kDiagnosticTopicCapacity);
    static_assert(kHealthBeacon.size() < kDiagnosticTopicCapacity);
    static_assert(kSafetyMonitorWarning.size() < kDiagnosticTopicCapacity);
    static_assert(kSafetyDegradedRequest.size() < kDiagnosticTopicCapacity);
    static_assert(kSafetySafeStopForced.size() < kDiagnosticTopicCapacity);

} // namespace safety_core::diag::topic
