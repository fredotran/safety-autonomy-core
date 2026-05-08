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

## 5. Verification Expectations
- Unit and integration tests must pass in CI.
- Fault-injection tests must be included in release qualification.
- Safety-case documents must be updated with requirement and risk deltas.

## 6. Out-of-Scope
This library does not by itself constitute ISO 3691 certification. Vehicle-level controls, sensing, braking performance validation, and operational procedures remain required at system level.
