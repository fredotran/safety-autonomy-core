#pragma once

#include "safety_core/common/time.hpp"
#include "safety_core/config/system_config.hpp"
#include "safety_core/platform/clock.hpp"
#include "safety_core/result.hpp"

#include <array>
#include <cstddef>
#include <functional>

namespace safety_core::exec
{

    template <std::size_t MaxTasks = 8U> class TaskExecutor
    {
      public:
        using TaskFn = std::function<Result(time::TimePoint)>;

        explicit TaskExecutor(platform::Clock* clock = nullptr) noexcept : clock_(clock) {}

        void set_clock(platform::Clock* clock) noexcept
        {
            clock_ = clock;
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

        Result add_task(TaskFn fn, time::Duration period) noexcept
        {
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
                    slot.used         = true;
                    slot.period       = use_period;
                    slot.next_release = now() + use_period;
                    slot.fn           = std::move(fn);
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
                    const auto deadline = slot.next_release + slot.period;
                    const Result r      = slot.fn(slot.next_release);
                    slot.next_release += slot.period;
                    if (!r.ok())
                    {
                        return r;
                    }
                    if (now() > deadline)
                    {
                        return Result::DeadlineMiss("task overran deadline");
                    }
                    if ((watchdog_period_.count() > 0) && ((now() - deadline) > watchdog_period_))
                    {
                        return Result::DeadlineMiss("watchdog window exceeded");
                    }
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
            TaskFn fn{};
        };

        std::array<TaskSlot, MaxTasks> tasks_{};
        platform::Clock* clock_{nullptr};
        const config::SystemConfig* config_{nullptr};
        std::size_t max_tasks_{MaxTasks};
        time::Duration default_period_{time::Duration::zero()};
        time::Duration watchdog_period_{time::Duration::zero()};
    };

} // namespace safety_core::exec
