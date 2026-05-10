#pragma once

// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT

#include "safety_core/common/time.hpp"
#include "safety_core/config/system_config.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/topics.hpp"
#include "safety_core/platform/clock.hpp"
#include "safety_core/result.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace safety_core::exec
{

    enum class CatchUpPolicy : std::uint8_t
    {
        SingleStep = 0U,
        BoundedCatchUp,
    };

    static_assert(88U < diag::kDiagnosticPayloadCapacity);
    static_assert(80U < diag::kDiagnosticPayloadCapacity);
    static_assert(64U < diag::kDiagnosticPayloadCapacity);
    static_assert(40U < diag::kDiagnosticPayloadCapacity);

    template <std::size_t MaxTasks = 8U> class TaskExecutor
    {
      public:
        using TaskFn = Result (*)(time::TimePoint, void*) noexcept;

        explicit TaskExecutor(platform::Clock* clock = nullptr) noexcept : clock_(clock) {}

        void set_clock(platform::Clock* clock) noexcept
        {
            clock_ = clock;
        }

        void set_diagnostic_transport(diag::DiagnosticTransport* transport) noexcept
        {
            diag_transport_ = transport;
        }

        void set_catch_up_policy(CatchUpPolicy policy, std::size_t max_runs_per_tick = 1U) noexcept
        {
            catch_up_policy_ = policy;
            catch_up_limit_  = (max_runs_per_tick == 0U) ? 1U : max_runs_per_tick;
        }

        Result apply_config(const config::SystemConfig& cfg) noexcept
        {
            config_ = &cfg;
            if (cfg.max_tasks > MaxTasks)
            {
                return Result::Fault("configured max tasks exceeds executor capacity");
            }
            max_tasks_       = static_cast<std::size_t>(cfg.max_tasks);
            default_period_  = cfg.timing.control_period;
            watchdog_period_ = cfg.timing.watchdog_period;
            return Result::Ok();
        }

        Result add_task(TaskFn fn, void* context, time::Duration period) noexcept
        {
            if (fn == nullptr)
            {
                return Result::InvalidState("task callback must be non-null");
            }
            const time::Duration use_period = (period.count() == 0) ? default_period_ : period;
            if (use_period.count() == 0)
            {
                return Result::InvalidState("task period must be non-zero");
            }
            std::size_t used_count = 0U;
            for (auto& slot : tasks_)
            {
                if (!slot.used)
                {
                    const time::TimePoint now_tp = now();
                    if ((use_period.count() > 0) && (now_tp > (time::TimePoint::max() - use_period)))
                    {
                        publish_event(diag::topic::kExecutorTaskError.data(), "reason=release_time_overflow");
                        return Result::Fault("task release time overflow");
                    }

                    slot.used         = true;
                    slot.period       = use_period;
                    slot.next_release = now_tp + use_period;
                    slot.fn           = fn;
                    slot.context      = context;
                    char payload[80]{};
                    static_cast<void>(std::snprintf(payload, sizeof(payload), "period_ns=%lld",
                                                    static_cast<long long>(slot.period.count())));
                    publish_event(diag::topic::kExecutorTaskAdded.data(), std::string_view(payload));
                    return Result::Ok();
                }
                ++used_count;
            }
            return (used_count >= max_tasks_) ? Result::Fault("task capacity exceeded")
                                              : Result::Fault("no task slots available");
        }

        Result run_due(time::TimePoint now_tp = time::now()) noexcept
        {
            for (auto& slot : tasks_)
            {
                if (!slot.used)
                {
                    continue;
                }
                if (now_tp >= slot.next_release)
                {
                    std::size_t run_count = 0U;
                    while (now_tp >= slot.next_release)
                    {
                        const auto release = slot.next_release;
                        if ((slot.period.count() > 0) && (release > (time::TimePoint::max() - slot.period)))
                        {
                            publish_event(diag::topic::kExecutorTaskError.data(), "reason=deadline_overflow");
                            return Result::Fault("task deadline overflow");
                        }

                        if ((watchdog_period_.count() > 0) && ((now_tp - release) > watchdog_period_))
                        {
                            char payload[64]{};
                            static_cast<void>(std::snprintf(payload, sizeof(payload), "lag_ns=%lld",
                                                            static_cast<long long>((now_tp - release).count())));
                            publish_event(diag::topic::kExecutorWatchdogExceeded.data(), std::string_view(payload));
                            return Result::DeadlineMiss("watchdog window exceeded");
                        }

                        const auto deadline = release + slot.period;
                        const Result r      = slot.fn(release, slot.context);
                        if ((slot.period.count() > 0) && (slot.next_release > (time::TimePoint::max() - slot.period)))
                        {
                            publish_event(diag::topic::kExecutorTaskError.data(), "reason=next_release_overflow");
                            return Result::Fault("task release progression overflow");
                        }
                        slot.next_release += slot.period;
                        ++run_count;
                        if (!r.ok())
                        {
                            char payload[40]{};
                            static_cast<void>(
                                std::snprintf(payload, sizeof(payload), "code=%d", static_cast<int>(r.code)));
                            publish_event(diag::topic::kExecutorTaskError.data(), std::string_view(payload));
                            return r;
                        }

                        const auto now_after = now();
                        if (now_after > deadline)
                        {
                            char payload[80]{};
                            static_cast<void>(
                                std::snprintf(payload, sizeof(payload), "deadline_ns=%lld",
                                              static_cast<long long>(deadline.time_since_epoch().count())));
                            publish_event(diag::topic::kExecutorDeadlineMiss.data(), std::string_view(payload));
                            return Result::DeadlineMiss("task overran deadline");
                        }

                        if ((catch_up_policy_ == CatchUpPolicy::SingleStep) || (run_count >= catch_up_limit_))
                        {
                            if ((catch_up_policy_ == CatchUpPolicy::BoundedCatchUp) && (now_tp >= slot.next_release))
                            {
                                char payload[88]{};
                                static_cast<void>(
                                    std::snprintf(payload, sizeof(payload), "remaining_lag_ns=%lld",
                                                  static_cast<long long>((now_tp - slot.next_release).count())));
                                publish_event(diag::topic::kExecutorCatchUpLimited.data(), std::string_view(payload));
                            }
                            break;
                        }
                    }

                    char payload[88]{};
                    static_cast<void>(
                        std::snprintf(payload, sizeof(payload), "next_release_ns=%lld",
                                      static_cast<long long>(slot.next_release.time_since_epoch().count())));
                    publish_event(diag::topic::kExecutorTaskRun.data(), std::string_view(payload));
                }
            }
            return Result::Ok();
        }

      private:
        [[nodiscard]] time::TimePoint now() const noexcept
        {
            return (clock_ != nullptr) ? clock_->now() : time::now();
        }

        struct TaskSlot
        {
            bool used{false};
            time::Duration period{};
            time::TimePoint next_release{};
            TaskFn fn{nullptr};
            void* context{nullptr};
        };

        std::array<TaskSlot, MaxTasks> tasks_{};
        platform::Clock* clock_{nullptr};
        const config::SystemConfig* config_{nullptr};
        diag::DiagnosticTransport* diag_transport_{nullptr};
        std::size_t max_tasks_{MaxTasks};
        time::Duration default_period_{time::Duration::zero()};
        time::Duration watchdog_period_{time::Duration::zero()};
        CatchUpPolicy catch_up_policy_{CatchUpPolicy::SingleStep};
        std::size_t catch_up_limit_{1U};

        void publish_event(const char* topic, std::string_view payload) const noexcept
        {
            if (diag_transport_ == nullptr)
            {
                return;
            }

            diag::DiagnosticEvent event{};
            event.set_topic(topic);
            event.set_payload(payload);
            event.timestamp_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now().time_since_epoch()).count());
            diag_transport_->publish(event);
        }
    };

} // namespace safety_core::exec
