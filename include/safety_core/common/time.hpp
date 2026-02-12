#pragma once

#include <chrono>

namespace safety_core::time {

using Duration = std::chrono::nanoseconds;
using TimePoint = std::chrono::time_point<std::chrono::steady_clock, Duration>;

inline TimePoint now() noexcept { return std::chrono::steady_clock::now(); }

struct Deadline {
    TimePoint due{};

    [[nodiscard]] constexpr bool expired(TimePoint t) const noexcept { return t >= due; }
    [[nodiscard]] bool expired() const noexcept { return expired(now()); }
};

class BudgetTracker {
public:
    explicit BudgetTracker(Duration budget) noexcept : budget_(budget), start_(now()) {}

    void restart(Duration budget) noexcept {
        budget_ = budget;
        start_ = now();
    }

    [[nodiscard]] Duration elapsed() const noexcept { return std::chrono::duration_cast<Duration>(now() - start_); }
    [[nodiscard]] bool within_budget() const noexcept { return elapsed() <= budget_; }
    [[nodiscard]] Duration remaining() const noexcept {
        const Duration used = elapsed();
        return (used >= budget_) ? Duration::zero() : (budget_ - used);
    }

private:
    Duration budget_;
    TimePoint start_;
};

}  // namespace safety_core::time
