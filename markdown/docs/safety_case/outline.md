# Safety Case Outline (Starter)

## 1) Scope and Operational Design Domain
- AGV/AMR indoor logistics operation.
- Mixed environment with pedestrians, forklifts, temporary occlusions/localization dropouts.

## 2) Top-Level Safety Claims
- C1: Core control/scheduling components are deterministic and bounded.
- C2: Unsafe actuation is constrained by validated envelopes and fault handling.
- C3: Runtime degradations are observable through diagnostics and beacon telemetry.

## 3) Hazards and Controls
- Refer to `docs/safety_case/traceability_matrix.md` for hazard-to-control mappings.

## 4) Verification Strategy
- Unit/regression tests per subsystem.
- Property-style stress tests for parsers and filters.
- Deterministic replay tests for diagnostic ordering.
- CI gates: static analysis, no-allocation symbol checks, coverage thresholds.

## 5) Assumptions and Dependencies
- Monotonic time source.
- Platform integration preserves bounded queue and transport semantics.
- Deployment preserves configured watchdog and control periods.

## 6) Residual Risk and Open Work
- Requirement IDs and hazard severity ranking to be frozen.
- Add MC/DC-oriented coverage evidence for selected safety-critical units.
- Extend motion primitive library to docking-specific and obstacle-avoidance maneuvers.
