#pragma once

#include <array>
#include <cstddef>
#include <functional>

#include "safety_core/common/time.hpp"
#include "safety_core/result.hpp"

namespace safety_core::exec {

template <std::size_t MaxTasks = 8U>
class TaskExecutor {
public:
    using TaskFn = std::function<Result(time::TimePoint)>;

    Result add_task(TaskFn fn, time::Duration period) noexcept {
        for (auto &slot : tasks_) {
            if (!slot.used) {
                slot.used = true;
                slot.period = period;
                slot.next_release = time::now() + period;
                slot.fn = std::move(fn);
                return Result::Ok();
            }
        }
        return Result::Fault("task capacity exceeded");
    }

    Result run_due(time::TimePoint now_tp = time::now()) noexcept {
        for (auto &slot : tasks_) {
            if (!slot.used) {
                continue;
            }
            if (now_tp >= slot.next_release) {
                const auto deadline = slot.next_release + slot.period;
                const Result r = slot.fn(slot.next_release);
                slot.next_release += slot.period;
                if (!r.ok()) {
                    return r;
                }
                if (time::now() > deadline) {
                    return Result::DeadlineMiss("task overran deadline");
                }
            }
        }
        return Result::Ok();
    }

private:
    struct TaskSlot {
        bool used{false};
        time::Duration period{};
        time::TimePoint next_release{};
        TaskFn fn{};
    };

    std::array<TaskSlot, MaxTasks> tasks_{};
};

}  // namespace safety_core::exec
