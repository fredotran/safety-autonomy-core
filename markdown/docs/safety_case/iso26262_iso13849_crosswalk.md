# ISO 26262 / ISO 13849 Crosswalk (Repository Scope)

## Purpose
Map repository-level safety work products and controls to commonly expected evidence domains in ISO 26262 and ISO 13849.

> This crosswalk supports audit preparation only. Formal compliance claims require project-specific tailoring, item definition, HARA/risk graph records, and independent assessment evidence.

| Evidence Domain | ISO 26262 (typical expectation) | ISO 13849 (typical expectation) | Repository Artifact(s) | Status |
|---|---|---|---|---|
| Functional safety management | Safety planning, responsibilities, confirmation measures. | Safety lifecycle planning, validation, and documented responsibility. | `docs/safety_case/functional_safety_plan.md` | In Progress |
| Hazard/risk analysis linkage | Item/hazard analysis linked to safety requirements and verification. | Risk reduction concept linked to safety functions and validation. | `docs/safety_case/traceability_matrix.md`, `docs/safety_case/residual_risk_register.md` | In Progress |
| Safety requirements decomposition | Technical safety requirements and rationale. | Safety-related control function definitions and category/performance rationale. | `docs/safety_case/traceability_matrix.md`, `docs/safety_case/safety_manual.md` | In Progress |
| Verification and validation strategy | V&V planning across unit/integration/release evidence. | Validation plan for safety-related parts of control systems. | `docs/safety_case/vv_strategy.md`, `docs/safety_case/verification_protocol_template.md` | In Progress |
| Tool confidence/qualification rationale | Tool confidence level argument and mitigation controls. | Confidence in tools affecting safety outputs. | `docs/safety_case/tool_confidence_register.md`, CI checks in `.gitlab-ci.yml` | In Progress |
| Change-impact and configuration control | Controlled impact analysis for safety-relevant changes. | Controlled modifications and revalidation evidence. | `docs/safety_case/change_impact_assessment_template.md`, release checklist | In Progress |
| Clause-level conformance package | Traceability to normative clause obligations. | Traceability to normative clause obligations. | `docs/safety_case/iso_3691_4_clause_matrix.md` plus this crosswalk | In Progress |

## Required Project-Level Inputs (outside repository-only scope)
1. Product/item definition and operational context accepted by safety authority.
2. Target integrity selection rationale (ASIL and/or PL/SIL target) with assumptions.
3. Hardware architecture diagnostics and fault metrics from system integration.
4. Independent functional safety assessment records.
5. Production, operation, and service procedures tied to safety requirements.
