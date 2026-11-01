# HIL (Hardware-in-the-Loop) Strategy

## Status: DEFERRED — accepted limitation

No hardware test rig exists for this project today. This document records the
intended SIL→HIL path and captures the absence as an accepted residual risk
(RR-005) rather than pretending coverage exists.

## Intended ladder

| Stage | Environment | What it proves |
|---|---|---|
| Unit/property tests | host, `ManualClock` | logic correctness, NaN/overflow guards |
| Fuzz harnesses | host, libFuzzer | parser/filter robustness |
| Benchmarks | host | p99/max latency vs budgets (order of magnitude) |
| In-process ROS 2 scenarios | container | envelope→supervisor→SafeStop chain, ~50 ms measured |
| Gazebo SIL | container | full stack incl. Nav2, sensors |
| **HIL (missing)** | real-time target + I/O stubs | true WCET, driver latency, watchdog behavior |

## Why HIL matters here

The claims that cannot be fully proven in software simulation:

1. **WCET on target** — host benchmarks give ~µs p99s, but real certification
   needs measurements on the deployment CPU under its scheduler/priorities.
2. **Driver/sensor timing** — actual LiDAR/odom arrival jitter, transport
   drops under load.
3. **Watchdog interaction** — the bounded executor's deadline behavior against
   a real RT scheduler.

## Minimal viable HIL (when hardware becomes available)

- A real-time Linux target (PREEMPT_RT) or an MCU running the
  `safety_core_certifiable` build.
- Loop back synthetic scan/odom through a socket/CAN stub; drive `ManualClock`
  replaced by `SteadyClock`; record cycle-time histograms.
- Reuse `benchmarks/safety_core_benchmarks.cpp` budgets as the pass criteria.

## Trigger to revisit

A concrete deployment target or certification customer justifies the
investment; until then the limitation is tracked and disclosed.
