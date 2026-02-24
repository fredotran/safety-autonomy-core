# Release Safety Evidence Checklist

## Purpose
Gate release candidates on safety-case completeness and reproducible verification evidence.

## Release Metadata
- Release ID:
- Commit SHA:
- Build profile:
- Safety reviewer:
- Date:

## A. CI and Build Integrity (required)
- [ ] `format_check` passed.
- [ ] `clang_tidy` passed.
- [ ] `policy_guard` passed.
- [ ] `build_and_test` passed.
- [ ] `coverage` passed at or above configured thresholds.
- [ ] `safety_case_guard` passed.

## B. Safety Case Documents (required)
- [ ] `docs/safety_case/traceability_matrix.md` updated for this release scope.
- [ ] `docs/safety_case/assumptions_register.md` reviewed; invalidated assumptions handled.
- [ ] `docs/safety_case/residual_risk_register.md` reviewed; Medium/High risks have approved mitigation plan.
- [ ] `docs/safety_case/iso_3691_4_clause_matrix.md` updated with clause impacts.
- [ ] `docs/safety_case/iso26262_iso13849_crosswalk.md` updated with impacted evidence domains.
- [ ] `docs/safety_case/functional_safety_plan.md` reviewed for role/activity changes.
- [ ] `docs/safety_case/vv_strategy.md` reviewed for verification campaign updates.
- [ ] `docs/safety_case/tool_confidence_register.md` reviewed for toolchain changes.
- [ ] `docs/safety_case/change_impact_assessment_template.md` instantiated for safety-relevant changes.
- [ ] `docs/safety_case/verification_protocol_template.md` instantiated for this release.

## C. Verification Evidence (required)
- [ ] Test run artifact archived (`ctest` output and environment info).
- [ ] Coverage report archived (`gcovr` summary and report files).
- [ ] Fault-injection evidence archived (`fault_injection_tests`, supervisor escalation paths).
- [ ] Configuration/startup evidence archived (`env_loader_tests`, `context_factory_tests`).
- [ ] No-allocation evidence archived (CI allocator symbol gate + `no_allocation_policy_tests`).

## D. Review and Approval (required)
- [ ] Code review completed for all safety-relevant changes.
- [ ] Safety review completed by independent reviewer.
- [ ] Residual risk acceptance documented by responsible owner.
- [ ] Release approval recorded.

## Sign-off
- Test Engineer:
- Safety Reviewer:
- Release Approver:
