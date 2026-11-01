# Safety Integration Manual (Starter)

## 1. Scope
This document provides integration constraints for using `safety_autonomy_core` in a safety-relevant mobile robot stack.

## 2. Mandatory Integration Requirements
1. Use a monotonic `platform::Clock` source for all runtime components.
2. Wire `ModeStateMachine`, `TaskExecutor`, and `SafetySupervisor` to the same diagnostic transport where feasible.
3. Configure watchdog period >= control period.
4. Provide valid motion envelope parameters (`max_speed`, `max_accel`, `max_comfort_decel`, non-negative latency/buffer).
5. Do not disable CI policy gates (`policy_guard`, allocator symbol checks, tests).

## 3. Supervisor Escalation Policy
- **Warning**: emits `safety.monitor_warning` diagnostic; no forced mode change.
- **Degraded**: emits `safety.degraded_request`; requests `Mode::Degraded`.
- **Critical**: emits `safety.safestop_forced`; latches fault and forces `Mode::SafeStop`.

## 4. Runtime Monitoring Inputs
- Clock sample progression (`observe_clock_sample`).
- Localization freshness (`observe_localization_age`).
- Diagnostic drop counts (`observe_transport_drop_count`).
- Sensor stream staleness (ROS 2 layer): scan watchdog (`scan_timeout_s` → EMERGENCY zone), odometry freshness (`odom_timeout_s` → conservative max-speed fallback), envelope-status monitor (`envelope_timeout_s` → fault `0xE002`). Streams that never produce data count as stale from node startup (grace period applies where configured).

## 5. Safety Fault Codes
Latched fault codes carried by `ModeStateMachine::fault_code()` and published on `safety/state`:

| Code | Source | Meaning | Latched response |
|---|---|---|---|
| `0x03E9` (1001) | SafetySupervisor | Platform clock moved backwards | Critical monitor event → fault latch + SafeStop |
| `0x03EA` (1002) | SafetySupervisor | Localization age exceeded `localization_timeout_s` (never-received odometry ages from node startup) | Critical monitor event → fault latch + SafeStop |
| `0x03EB` (1003) | SafetySupervisor | Diagnostic transport drop count exceeded | Warning → Critical escalation → fault latch + SafeStop |
| `0xE001` | SafetySupervisorNode | Safety envelope reported Emergency zone | Fault latch + SafeStop command publish |
| `0xE002` | SafetySupervisorNode | `safety/envelope_status` stream stale or lost (`envelope_timeout_s`) | Fault latch + SafeStop command publish |

## 6. Verification Expectations
- Unit and integration tests must pass in CI.
- Fault-injection tests must be included in release qualification.
- Safety-case documents must be updated with requirement and risk deltas.

## 7. Out-of-Scope
This library does not by itself constitute ISO 3691 certification. Vehicle-level controls, sensing, braking performance validation, and operational procedures remain required at system level.
