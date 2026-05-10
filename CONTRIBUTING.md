# Contributing to Safety Autonomy Core

Thank you for your interest in contributing to Safety Autonomy Core! This document provides guidelines for contributing to this safety-critical robotics library.

## 🎯 Project Philosophy

This repository targets safety-minded, deterministic C++ behavior. We prioritize:
- **Minimal changes** over broad refactors
- **Deterministic behavior** in all runtime paths
- **Bounded operations** (no dynamic allocation in critical paths)
- **Code quality** with clang-format and warning-free builds
- **Test-driven development** with comprehensive test coverage

---

## 🚀 Getting Started

### 1. Fork and Clone

```bash
# Fork the repository on GitHub
git clone https://github.com/your-username/safety-autonomy-core.git
cd safety-autonomy-core
git remote add upstream https://github.com/fredotran/safety-autonomy-core.git
```

### 2. Set Up Development Environment

```bash
# Enable git hooks
git config core.hooksPath .githooks

# Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DSAFETY_CORE_ENABLE_SANITIZERS=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

### 3. Create a Feature Branch

```bash
git checkout -b feature/your-feature-name
```

---

## 📝 Development Guidelines

### Code Quality Standards

**Do:**
- Keep changes small and focused
- Add tests for new behavior
- Preserve deterministic behavior
- Maintain clang-format compliance
- Keep code warning-free
- Document non-obvious safety logic

**Don't:**
- Introduce dynamic allocation in critical paths
- Make broad refactors without clear justification
- Add unnecessary dependencies
- Bundle formatting-only edits with behavior changes
- Commit generated artifacts (gcov, coverage outputs)

### Safety-Critical Considerations

When modifying safety behavior:
1. Update relevant tests
2. Document interface changes clearly
3. Update safety case documentation in `markdown/docs/safety_case/`
4. Ensure policy guard passes (`tools/safety/verify_policy.sh`)

---

## 🧪 Testing Requirements

### Before Submitting

Run the quality gate script:

```bash
# Standard quality gate
tools/dev/run_quality_gate.sh

# With coverage verification
tools/dev/run_quality_gate.sh --coverage
```

### Test Coverage

- Add tests for new or modified behavior
- Ensure existing tests still pass
- Maintain coverage thresholds (80% line, 55% branch)

### Test Categories

- **Unit Tests**: Component-level testing
- **Property Tests**: Safety property verification
- **Integration Tests**: Component interaction testing
- **Fault Injection**: Robustness under failure conditions

---

## ✅ Pull Request Checklist

Before submitting a PR, ensure:

- [ ] Build succeeds locally
- [ ] Relevant tests pass locally
- [ ] Policy guard passes (`tools/safety/verify_policy.sh`)
- [ ] Documentation updated for behavior/API changes
- [ ] No generated artifacts committed
- [ ] Commit messages are descriptive and scoped
- [ ] Safety case docs updated if applicable

---

## 📋 Commit Guidelines

### Commit Message Format

Use descriptive, scoped commit messages:

```
module: brief description of change

Detailed explanation of why and what changed.
```

Examples:
- `state_machine: Add docking mode with transition rules`
- `filters: Fix innovation gating in bounded EKF`
- `docs: Update README with new installation instructions`

### Commit Hygiene

- Keep commits focused on one concern
- Avoid mixing formatting with behavior changes
- Use present tense for commit messages
- Reference issue numbers when applicable

---

## 🔒 Safety Case Documentation

When adding or modifying safety behavior, update relevant documentation in `markdown/docs/safety_case/`:

- **traceability_matrix.md** - Requirement traceability
- **assumptions_register.md** - System assumptions
- **residual_risk_register.md** - Risk assessment
- **safety_manual.md** - Safety procedures

---

## 🤝 Code Review Process

### Review Expectations

- Reviewers focus on safety-critical correctness
- All changes must pass the quality gate
- Safety guards have veto power over non-compliant changes
- Be responsive to review feedback

### Review Timeline

- Expect review within 2-3 business days
- Complex safety changes may take longer
- Ping reviewers after 5 days with no response

---

## 🚨 Common Issues

### Build Failures

**Problem**: Build fails with warnings
- **Solution**: Fix warnings and rebuild

**Problem**: Tests fail locally
- **Solution**: Ensure your environment matches CI (sanitizers, compiler version)

### Policy Guard Failures

**Problem**: Policy guard detects banned APIs
- **Solution**: Review `tools/safety/verify_policy.sh` and use alternatives

### CI Failures

**Problem**: CI fails but local build passes
- **Solution**: Check for environment differences, ensure git hooks are enabled

---

## 📞 Getting Help

### Resources

- **Documentation**: See [OVERVIEW.md](OVERVIEW.md) for project overview
- **CI/CD**: See [CI_DOCUMENTATION.md](CI_DOCUMENTATION.md) for pipeline details
- **Architecture**: See inline code documentation and comments
- **Issues**: Open GitHub issues for bugs or questions

### Communication

- Use GitHub Discussions for questions and ideas
- Open issues for bug reports
- Use pull requests for code contributions
- Be patient with reviews (safety-critical code requires careful review)

---

## 🎉 Recognition

Contributors are recognized in:
- **CHANGELOG.md** for significant contributions
- Release notes for feature additions
- Contributor acknowledgments in major releases

Thank you for contributing to safety-critical robotics! 🤖
