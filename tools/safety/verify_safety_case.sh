#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

echo "[safety_case] Verifying required safety-case artifacts"

required_files=(
  "markdown/docs/safety_case/traceability_matrix.md"
  "markdown/docs/safety_case/assumptions_register.md"
  "markdown/docs/safety_case/residual_risk_register.md"
  "markdown/docs/safety_case/safety_manual.md"
  "markdown/docs/safety_case/verification_protocol_template.md"
  "markdown/docs/safety_case/iso_3691_4_clause_matrix.md"
  "markdown/docs/safety_case/release_safety_evidence_checklist.md"
  "markdown/docs/safety_case/iso26262_iso13849_crosswalk.md"
  "markdown/docs/safety_case/functional_safety_plan.md"
  "markdown/docs/safety_case/vv_strategy.md"
  "markdown/docs/safety_case/tool_confidence_register.md"
  "markdown/docs/safety_case/change_impact_assessment_template.md"
)

for file in "${required_files[@]}"; do
  if [[ ! -f "$file" ]]; then
    echo "[FAIL] Missing safety-case artifact: $file" >&2
    exit 1
  fi
done

if ! grep -q "Known Gaps To Close Before Certification Claim" markdown/docs/safety_case/iso_3691_4_clause_matrix.md; then
  echo "[FAIL] Clause matrix missing certification-gap section" >&2
  exit 1
fi

if ! grep -q "## A. CI and Build Integrity" markdown/docs/safety_case/release_safety_evidence_checklist.md; then
  echo "[FAIL] Release checklist missing CI gate section" >&2
  exit 1
fi

if ! grep -q "ISO 26262" markdown/docs/safety_case/iso26262_iso13849_crosswalk.md; then
  echo "[FAIL] Crosswalk missing ISO 26262 coverage" >&2
  exit 1
fi

if ! grep -q "ISO 13849" markdown/docs/safety_case/iso26262_iso13849_crosswalk.md; then
  echo "[FAIL] Crosswalk missing ISO 13849 coverage" >&2
  exit 1
fi

if ! grep -q "## 2. Roles and Responsibilities" markdown/docs/safety_case/functional_safety_plan.md; then
  echo "[FAIL] Functional safety plan missing roles section" >&2
  exit 1
fi

echo "[safety_case] OK"
