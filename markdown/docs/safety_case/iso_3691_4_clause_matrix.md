# ISO 3691-4 Clause Conformance Matrix (Starter)

## Purpose
Provide clause-level linkage between ISO 3691-4 safety obligations, repository controls, verification evidence, and release artifacts.

> Note: This is a living matrix for audit preparation. Clause text must be validated against the licensed standard copy used by the safety team.

| Clause Reference | Safety Intent (summary) | Repository Control(s) | Verification Evidence | Status | Evidence Artifact(s) |
|---|---|---|---|---|---|
| 4.x (Risk assessment and lifecycle process) | Hazard identification, risk treatment, and controlled updates across lifecycle. | Safety case package, assumptions register, residual risk register, traceability matrix. | Document review + release checklist sign-off. | In Progress | `docs/safety_case/traceability_matrix.md`, `docs/safety_case/assumptions_register.md`, `docs/safety_case/residual_risk_register.md`, `docs/safety_case/release_safety_evidence_checklist.md` |
| 5.x (Safety-related control functions) | Deterministic transition to safe state on critical faults/degradation. | `safety::SafetySupervisor` escalation to `Mode::SafeStop`; watchdog and localization freshness monitors. | Unit tests + fault-injection scenarios. | In Progress | `tests/safety_supervisor_tests.cpp`, `tests/fault_injection_tests.cpp`, `tests/system_context_tests.cpp` |
| 5.x (Motion limiting and stopping behavior) | Bounded motion and safe stopping envelope checks. | `motion::TrajectoryValidator`; safety envelope evaluation utilities; bounded control outputs. | Unit/property tests for bounds and envelope violations. | In Progress | `tests/motion_trajectory_tests.cpp`, `tests/safety_envelope_tests.cpp`, `tests/pid_controller_tests.cpp` |
| 6.x (Monitoring, diagnostics, operator awareness) | Detect and surface degraded conditions and observability gaps. | Health beacon publisher, truncation flags/counters, monitor diagnostics topics. | Beacon/truncation/executor diagnostics tests. | In Progress | `tests/health_beacon_tests.cpp`, `tests/diagnostic_truncation_tests.cpp`, `tests/executor_watchdog_tests.cpp` |
| 7.x (Configuration and integration control) | Controlled configuration, migration, validation, and startup behavior. | `build_context` env load + migration + strict/warning validation policy. | Config/env parsing and startup matrix tests. | In Progress | `tests/env_loader_tests.cpp`, `tests/context_factory_tests.cpp` |
| 8.x (Verification and validation evidence) | Repeatable verification campaign with documented preconditions and approvals. | Verification protocol template and release evidence checklist. | CI gate status + signed release protocol record. | In Progress | `docs/safety_case/verification_protocol_template.md`, `docs/safety_case/release_safety_evidence_checklist.md`, `.gitlab-ci.yml` |

## Known Gaps To Close Before Certification Claim
1. Complete clause-by-clause mapping with exact paragraph references from the normative standard.
2. Attach objective evidence artifacts (test logs, coverage reports, review records, change-impact analysis) per clause row.
3. Extend structural coverage argumentation for safety-critical units (e.g., MC/DC-oriented campaign where required by chosen assurance target).
4. Add independent safety review sign-off records for each release candidate.
