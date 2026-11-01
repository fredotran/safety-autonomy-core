# Residual Risk Register

## Purpose
Tracks known residual safety risks that remain after implemented controls, plus mitigation and ownership.

| Risk ID | Description | Current Controls | Residual Risk | Next Mitigation | Owner |
|---|---|---|---|---|---|
| RR-001 | Vehicle-level sensing blind spots may miss fast dynamic obstacles. | Runtime trajectory envelope checks, supervisor safe-stop escalation. | Medium | Add system-level perception redundancy and safety-rated field monitoring evidence. | Platform Integration |
| RR-002 | Transport-level observability drops can hide degraded state onset. | Diagnostic drop monitoring and warning/critical escalation in `SafetySupervisor`. | Medium | Add persistent drop-rate telemetry and alarm routing in deployment stack. | Diagnostics Integration |
| RR-003 | Config mismatch between tested defaults and deployed values. | Startup validation + migration + CI checks. | Low/Medium | Enforce signed configuration manifest and runtime hash check. | Release Engineering |
| RR-004 | Clause-level ISO 3691-4 evidence package is incomplete. | Hazard/requirement traceability matrix and test coverage baseline. | Medium/High | Complete clause-by-clause conformance matrix and third-party audit package. | Safety Case Lead |
| RR-005 | No hardware-in-the-loop evidence: true WCET, driver timing and watchdog behavior unproven on target. | Host benchmarks (p99/max vs budgets), in-process ROS 2 scenario latency measurement, SIL/Gazebo. | Medium | Deploy `safety_core_certifiable` on a PREEMPT_RT target/MCU when hardware scope is defined. See `hil_strategy.md`. | Platform Integration |
| RR-006 | No certified MISRA tool (PC-lint/Polyspace/QAC); open-source coverage approximates ~60-70% of rules. | cppcheck+clang-tidy blocking gate with baseline ratchet, `-fno-exceptions/-fno-rtti` certifiable target, sanitizer+fuzz CI. | Medium | Procure commercial tool upon certification contract; maintain zero-new-finding ratchet meanwhile. See `misra_status.md`. | Safety Case Lead |

## Review Cadence
- Review each release candidate.
- Escalate any Medium/High risk that lacks an approved mitigation plan.
