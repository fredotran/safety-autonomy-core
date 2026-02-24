# Diagnostics and Watchdog Integration Guide

## Purpose
This guide defines a deterministic integration pattern for watchdog-aware scheduling and periodic health beacons.

## 1) Wire transport and health monitor
- Provide a `diag::DiagnosticTransport` implementation (logging, ring buffer, or platform adapter).
- Optionally provide a `diag::HealthMonitor` implementation for mode/fault notifications.
- Pass both via `system::ContextFactoryOptions` so `build_context(...)` can wire all core components consistently.

## 2) Configure executor watchdog
- Set `timing.watchdog_period` to a bounded value larger than nominal task runtime.
- Keep `timing.control_period` deterministic and fixed.
- Use `CatchUpPolicy::BoundedCatchUp` for burst resilience while preserving bounded execution.

## 3) Publish periodic beacons
- Instantiate `diag::HealthBeaconPublisher` with your transport.
- Publish at a fixed period using control-loop time (not wall-clock polling).
- Include at minimum:
  - current mode
  - fault-latched flag
  - watchdog lag
  - truncation counters

## 4) Required diagnostic topics
- `executor.watchdog_exceeded`
- `executor.deadline_miss`
- `executor.catch_up_limited`
- `health.beacon`

## 5) Safety checks before release
- Verify no-allocation policy tests pass.
- Verify deterministic replay tests pass.
- Verify watchdog fault path and beacon emission are present in integration logs.

## 6) Operational recommendations
- Alert when watchdog overruns exceed threshold per minute.
- Alert when truncation counters increase unexpectedly.
- Store beacon payloads in a bounded telemetry channel for post-incident replay.
