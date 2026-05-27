# Sub-Project 2: Code Quality & Refactoring — Implementation Plan

**Date**: 2026-05-27  
**Priority**: High  
**Branch**: main  
**Status**: PENDING

---

## Goal

Reduce complexity in `safety_supervisor_node` (552 lines) and `sensor_monitor_node` (444 lines)
by extracting cohesive state groups into focused structs, replacing suppressed result-casts
with a traceable `log_result()` helper, and adding per-tick timing telemetry.

---

## Context

Repository: `safety-autonomy-core`  
Docker container: `safety-autonomy-demo` (workspace `/workspace/ros2_ws`)

Key files:
- `ros2/src/safety_core_ros/include/safety_core_ros/safety_supervisor_node.hpp`
- `ros2/src/safety_core_ros/src/safety_supervisor_node.cpp`
- `ros2/src/safety_core_ros/include/safety_core_ros/sensor_monitor_node.hpp`
- `ros2/src/safety_core_ros/src/sensor_monitor_node.cpp`

Result type: `safety_core::Result` in `include/safety_core/result.hpp`  
- Has `.ok()` bool, `.code` enum, `.message` string_view

Current pain points:
1. 20+ `(void)machine_->` casts suppress return-value warnings silently
2. Sensor recovery state (5 members + 5 methods) is entangled in the node class
3. Speed-limit ramp state (5 members + 1 method) is entangled in the node class
4. No per-tick timing visibility

---

## Tasks

### Task 1 — `log_result()` helper (SP2-T1)

**Scope**: `safety_supervisor_node.hpp` + `safety_supervisor_node.cpp`

**What to add** in the `private:` section of `SafetySupervisorNode`:

```cpp
// In safety_supervisor_node.hpp, private section:
inline void log_result(const safety_core::Result& r, const char* ctx) const noexcept
{
    if (!r.ok())
    {
        RCLCPP_WARN(get_logger(), "[%s] state-machine call failed: %s (code=%d)",
                    ctx, r.message.data(), static_cast<int>(r.code));
    }
}
```

**What to change** in `safety_supervisor_node.cpp` — replace every `(void)machine_->` and
`(void)supervisor_->` cast with a `log_result()` call. Example:

```cpp
// Before:
(void)machine_->transition_to(Mode::Idle);

// After:
log_result(machine_->transition_to(Mode::Idle), "init: transition_to(Idle)");
```

Full list of casts to replace (line numbers from current file):
- Line 129: `(void)machine_->transition_to(Mode::Idle)` → init context
- Line 159: `(void)machine_->transition_to(Mode::Moving)` → on_envelope context
- Line 178: `(void)machine_->transition_to(Mode::Moving)` → handle_zone_transition/clear context
- Line 187: `(void)machine_->request_obstacle_hold()` → handle_zone_transition/protective context
- Line 194: `(void)machine_->latch_fault(0xE001U)` → handle_zone_transition/emergency context
- Line 195: `(void)machine_->transition_to(Mode::SafeStop)` → handle_zone_transition/emergency context
- Line 221: `(void)supervisor_->observe_clock_sample(...)` → timer_tick context
- Line 320: `(void)machine_->latch_fault(0xE002U)` → check_sensor_health/fault context
- Line 321: `(void)machine_->transition_to(Mode::SafeStop)` → check_sensor_health/fault context
- Line 331: `(void)machine_->transition_to(Mode::Degraded)` → check_sensor_health/degraded context
- Line 343: `(void)machine_->transition_to(Mode::Moving)` → check_sensor_health/recovered context
- Line 359: `(void)supervisor_->observe_localization_age(...)` → check_localization_staleness context
- Line 365: `(void)machine_->report_localization_lost()` → check_localization_staleness context
- Line 377: `(void)machine_->transition_to(Mode::Degraded)` → handle_sensor_degraded context
- Line 395: `(void)machine_->transition_to(Mode::Moving)` → handle_sensor_recovered context
- Line 450: `(void)machine_->latch_fault(0xE003U)` → check_sensor_recovery_timeouts context
- Line 451: `(void)machine_->transition_to(Mode::SafeStop)` → check_sensor_recovery_timeouts context

Keep `(void)request;` (line 535) — this is an intentional unused parameter suppression, not a result cast.

**Verification**: Build in Docker, all 16 tests pass, no new warnings.

---

### Task 2 — `SensorRecoveryManager` struct (SP2-T2)

**Scope**: New header `include/safety_core_ros/sensor_recovery_manager.hpp`, update `safety_supervisor_node.hpp` and `.cpp`

Create a standalone `SensorRecoveryManager` struct in a new header that encapsulates all
sensor-recovery state and logic, removing it from the node class.

**New file** `ros2/src/safety_core_ros/include/safety_core_ros/sensor_recovery_manager.hpp`:

The struct must hold:
- `std::unordered_map<std::string, SensorRecoveryInfo> sensor_states_` (move from node)
- `bool in_degraded_mode_{false}` (move from node)
- The `SensorRecoveryInfo` struct definition (move from node header)

The struct must expose these methods (signatures matching the current private node methods):
```cpp
struct SensorRecoveryManager {
    struct SensorRecoveryInfo { ... }; // moved from node
    
    // Called from on_sensor_health callback
    // Returns pair<bool degraded_triggered, bool recovered_triggered>
    struct HealthEvent { bool degraded; bool recovered; std::string sensor_name; };
    std::optional<HealthEvent> process_health_msg(const std::string& name, uint8_t status, uint64_t now_ns);
    
    // Called from timer_tick (check_sensor_recovery_timeouts)
    // Returns sensor name that timed out, if any
    std::optional<std::string> check_timeouts(uint64_t now_ns, uint64_t recovery_timeout_ns);
    
    bool is_any_degraded() const noexcept;
    void mark_degraded_mode(bool val) { in_degraded_mode_ = val; }
    bool in_degraded_mode() const noexcept { return in_degraded_mode_; }
    
    std::unordered_map<std::string, SensorRecoveryInfo> sensor_states_;
    bool in_degraded_mode_{false};
};
```

**Update `safety_supervisor_node.hpp`**:
- Add `#include "safety_core_ros/sensor_recovery_manager.hpp"`
- Replace `SensorRecoveryInfo` nested struct with `SensorRecoveryManager recovery_mgr_` member
- Remove `bool in_degraded_mode_` member (moved into manager)
- Remove `sensor_states_` member (moved into manager)
- Keep `handle_sensor_degraded()`, `handle_sensor_recovered()`, `is_any_sensor_degraded()` private methods OR move them into the manager — prefer moving the logic into the manager

**Update `safety_supervisor_node.cpp`**:
- Replace inline sensor state manipulation with calls to `recovery_mgr_` methods
- In `on_sensor_health`: delegate to `recovery_mgr_.process_health_msg()`
- In `check_sensor_recovery_timeouts`: delegate to `recovery_mgr_.check_timeouts()`
- In `handle_sensor_degraded` / `handle_sensor_recovered`: use `recovery_mgr_.in_degraded_mode()` / `recovery_mgr_.mark_degraded_mode()`
- In `is_any_sensor_degraded()`: delegate to `recovery_mgr_.is_any_degraded()`

**Verification**: Build in Docker, all 16 tests pass.

---

### Task 3 — `SpeedLimitController` struct (SP2-T3)

**Scope**: New header `include/safety_core_ros/speed_limit_controller.hpp`, update `safety_supervisor_node.hpp` and `.cpp`

Create a focused `SpeedLimitController` struct encapsulating speed-ramp state and logic.

**New file** `ros2/src/safety_core_ros/include/safety_core_ros/speed_limit_controller.hpp`:

```cpp
// Copyright (c) 2026 RedEarth OS
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <optional>

namespace safety_core_ros
{

struct SpeedLimitController
{
    // Initialise with max speed and ramp duration
    explicit SpeedLimitController(double max_speed_mps, uint64_t ramp_ns)
        : max_speed_mps_(max_speed_mps)
        , current_limit_mps_(max_speed_mps)
        , target_limit_mps_(max_speed_mps)
        , ramp_ns_(ramp_ns)
    {}

    // Call on each timer tick; returns updated current limit
    double update(uint64_t now_ns, bool safe_stop_active);

    // Trigger speed degradation (50% of max)
    void on_degraded();

    // Start ramp back to max speed
    void on_recovered(uint64_t now_ns);

    // Force to zero (safe stop)
    void on_safe_stop();

    [[nodiscard]] double current_limit() const noexcept { return current_limit_mps_; }

private:
    double max_speed_mps_;
    double current_limit_mps_;
    double target_limit_mps_;
    uint64_t ramp_ns_;
    std::optional<uint64_t> ramp_start_ns_;
};

} // namespace safety_core_ros
```

**Update `safety_supervisor_node.hpp`**:
- Add `#include "safety_core_ros/speed_limit_controller.hpp"`
- Replace speed-limit state members with `SpeedLimitController speed_ctrl_` member:
  - Remove `double current_speed_limit_mps_`
  - Remove `double target_speed_limit_mps_`
  - Remove `std::optional<std::uint64_t> speed_ramp_start_ns_`
  - Remove `std::uint64_t recovery_speed_ramp_ns_`

**Update `safety_supervisor_node.cpp`**:
- Initialise `speed_ctrl_(params_.max_speed_mps, recovery_speed_ramp_ns_)` in constructor
- In `update_speed_limit()`: delegate to `speed_ctrl_.update(now_ns, mode == Mode::SafeStop)`
- In `handle_sensor_degraded()`: call `speed_ctrl_.on_degraded()`
- In `handle_sensor_recovered()` (when no more degraded): call `speed_ctrl_.on_recovered(now_ns())`
- In `publish_recovery_speed_limit()`: use `speed_ctrl_.current_limit()`

**Verification**: Build in Docker, all 16 tests pass.

---

### Task 4 — Timer-tick telemetry (SP4)

**Scope**: `safety_supervisor_node.hpp` + `safety_supervisor_node.cpp`

Add non-intrusive per-tick timing so operators can detect scheduler jitter.

**What to add in `.hpp`** (private members after `timer_`):
```cpp
// Tick duration telemetry
std::uint64_t tick_count_{0U};
double tick_sum_ms_{0.0};
double tick_max_ms_{0.0};
static constexpr std::uint64_t kTelemetryInterval = 200U; // every 200 ticks ≈ 10s at 20Hz
```

**What to add in `.cpp`** at start and end of `timer_tick()`:
```cpp
void SafetySupervisorNode::timer_tick()
{
    const auto tick_start = std::chrono::steady_clock::now();
    
    // ... existing body unchanged ...
    
    // Telemetry
    const double tick_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - tick_start).count();
    tick_sum_ms_ += tick_ms;
    tick_max_ms_ = std::max(tick_max_ms_, tick_ms);
    ++tick_count_;
    if (tick_count_ % kTelemetryInterval == 0U)
    {
        const double avg_ms = tick_sum_ms_ / static_cast<double>(kTelemetryInterval);
        RCLCPP_DEBUG(get_logger(), "tick telemetry | avg=%.3fms max=%.3fms count=%lu",
                     avg_ms, tick_max_ms_, tick_count_);
        publish_diagnostic_event("safety/tick_telemetry",
            "avg_ms=" + std::to_string(avg_ms) + " max_ms=" + std::to_string(tick_max_ms_));
        tick_sum_ms_ = 0.0;
        tick_max_ms_ = 0.0;
    }
}
```

Add `#include <chrono>` if not already present (it is already in the .cpp).

**Verification**: Build in Docker, all 16 tests pass, `ros2 topic echo /safety/diagnostics` shows tick_telemetry events after ~10s.

---

## Commit Strategy

Squash into a single commit per task (4 commits total):
```
refactor(sp2-t1): Add log_result() helper, replace (void) suppressed casts
refactor(sp2-t2): Extract SensorRecoveryManager from SafetySupervisorNode
refactor(sp2-t3): Extract SpeedLimitController from SafetySupervisorNode
feat(sp4): Add timer-tick duration telemetry to SafetySupervisorNode
```

---

## Definition of Done

- [ ] All 16 tests pass in Docker container
- [ ] Zero new compiler warnings introduced
- [ ] `safety_supervisor_node.cpp` is under 450 lines after T2+T3
- [ ] Commits squashed and pushed to main
