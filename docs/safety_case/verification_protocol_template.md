# Verification Protocol Template

## 1. Test Campaign Metadata
- Campaign ID:
- Software version / commit:
- Build profile:
- Date / operator:

## 2. Preconditions
- CI baseline green (`format`, `lint`, `policy_guard`, `build_and_test`, `coverage`).
- Deployment configuration checksum recorded.
- Safety assumptions register reviewed.

## 3. Requirement-Based Verification Matrix
| Requirement ID | Test/Procedure | Expected Outcome | Pass/Fail | Evidence Artifact |
|---|---|---|---|---|
| SR-CTRL-001 | `pid_controller_tests` | Stale localization output clamps to safe value |  |  |
| SR-EXEC-001 | `executor_watchdog_tests` | Deadline/watchdog misses detected and surfaced |  |  |
| SR-MOTION-001 | `motion_trajectory_tests` | Envelope and bounds violations rejected with indexed reason |  |  |
| SR-SUP-001 | `safety_supervisor_tests` | Critical events force safe-stop deterministically |  |  |

## 4. Fault Injection Protocol
- Clock regression scenario
- Diagnostic transport drop scenario
- Sensor NaN burst scenario
- Record supervisor escalation and mode transitions

## 5. Sign-off
- Test Engineer:
- Safety Reviewer:
- Release Approver:
