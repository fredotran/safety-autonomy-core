# Tool Confidence Register (Starter)

## Purpose
Track tools that can influence safety evidence or safety-relevant binaries and define confidence controls.

| Tool | Usage in Safety Workflow | Potential Impact | Confidence Controls | Owner |
|---|---|---|---|---|
| CMake/Ninja | Build configuration and compilation orchestration. | Misconfiguration can change generated binaries. | Version pinning in CI image, reproducible build presets, build smoke tests. | Build/Release |
| clang-format | Style enforcement. | Low direct safety impact; indirect review readability impact. | CI format gate, repo-level config. | Development |
| clang-tidy | Static analysis and defect detection. | Missed findings if misconfigured. | CI lint gate, warnings as errors policy for build profiles. | Development |
| gcovr/gcc coverage toolchain | Coverage evidence reporting. | Incorrect metrics can misstate verification confidence. | CI threshold gate, report archiving, periodic manual review. | Safety Case Lead |
| Policy scripts (`verify_policy.sh`) | Banned API guard in core code. | Script errors can miss prohibited constructs. | CI gate + script review + negative tests in merge requests. | Safety Case Lead |
| Safety-case script (`verify_safety_case.sh`) | Ensures required evidence artifacts exist. | Missing artifact checks can allow incomplete release packages. | CI gate + checklist cross-check + review. | Safety Case Lead |

## Review Rules
- Review this register on each release candidate.
- Add newly introduced safety-relevant tools before they are used in release workflows.
