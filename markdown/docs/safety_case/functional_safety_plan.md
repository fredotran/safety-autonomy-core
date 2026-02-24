# Functional Safety Plan (Starter)

## 1. Scope
This plan defines repository-level functional safety activities for safety-related software in `safety_autonomy_core`.

## 2. Roles and Responsibilities
- **Safety Case Lead**: owns safety-case integrity and release evidence completeness.
- **Module Owner**: ensures requirement-to-test traceability for changed modules.
- **Independent Safety Reviewer**: performs independent review of safety-relevant changes.
- **Release Approver**: accepts residual risks and signs release checklist.

## 3. Required Activities per Safety-Relevant Change
1. Update requirement and hazard traceability entries.
2. Perform change-impact analysis using the change-impact template.
3. Execute and archive verification evidence (tests, coverage, policy guards).
4. Update residual risk and assumptions registers if affected.
5. Obtain independent safety review sign-off.

## 4. Configuration and Change Control
- Safety-relevant changes must reference affected requirement IDs.
- Safety-case artifacts and code changes should be reviewed in the same merge request.
- Release candidates must include a completed release safety evidence checklist.

## 5. Verification Gates
- CI gates: `format_check`, `clang_tidy`, `policy_guard`, `safety_case_guard`, `build_and_test`, `coverage`.
- Release gate: completed `release_safety_evidence_checklist.md` with sign-off.

## 6. Audit Readiness Outputs
- Updated traceability matrix and clause crosswalk.
- Completed verification protocol instance.
- Archived test and coverage evidence.
- Signed release safety evidence checklist.
