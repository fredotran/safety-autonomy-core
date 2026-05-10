#!/bin/bash

# CI Workflow Validation Script
# This script validates the CI workflow structure and logic

# Don't exit on error, we want to run all tests
set +e

echo "🔍 Validating CI Workflow Structure..."
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
pass_test() {
    echo -e "${GREEN}✅ PASS${NC}: $1"
    ((TESTS_PASSED++))
}

fail_test() {
    echo -e "${RED}❌ FAIL${NC}: $1"
    ((TESTS_FAILED++))
}

warn_test() {
    echo -e "${YELLOW}⚠️  WARN${NC}: $1"
}

# Test 1: YAML syntax validation
echo "Test 1: YAML Syntax Validation"
if python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))" 2>/dev/null; then
    pass_test "YAML syntax is valid"
else
    fail_test "YAML syntax is invalid"
fi
echo ""

# Test 2: Check for required stages
echo "Test 2: Required Stages Check"
REQUIRED_STAGES=("detect_changes" "format_check" "clang_tidy" "hook_smoke" "policy_guard" "safety_case_guard" "build_and_test" "coverage" "docker_build" "ci_summary")

for stage in "${REQUIRED_STAGES[@]}"; do
    if grep -q "^  $stage:" .github/workflows/ci.yml; then
        pass_test "Stage '$stage' exists"
    else
        fail_test "Stage '$stage' is missing"
    fi
done
echo ""

# Test 3: Check job dependencies
echo "Test 3: Job Dependencies Check"
if grep -q "needs: detect_changes" .github/workflows/ci.yml; then
    pass_test "Jobs depend on detect_changes"
else
    fail_test "Jobs should depend on detect_changes"
fi

if grep -q "needs: docker_build" .github/workflows/ci.yml; then
    pass_test "Integration tests depend on docker_build"
else
    fail_test "Integration tests should depend on docker_build"
fi
echo ""

# Test 4: Check safety guards always run
echo "Test 4: Safety Guards Always Run"
if grep -A 5 "policy_guard:" .github/workflows/ci.yml | grep -q "if: always()"; then
    pass_test "Policy guard has always() condition"
else
    warn_test "Policy guard might not always run"
fi

if grep -A 5 "safety_case_guard:" .github/workflows/ci.yml | grep -q "if: always()"; then
    pass_test "Safety case guard has always() condition"
else
    warn_test "Safety case guard might not always run"
fi
echo ""

# Test 5: Check caching strategy
echo "Test 5: Caching Strategy Check"
if grep -q "CACHE_VERSION" .github/workflows/ci.yml; then
    pass_test "Cache versioning is implemented"
else
    fail_test "Cache versioning is missing"
fi

if grep -q "ccache" .github/workflows/ci.yml; then
    pass_test "ccache caching is implemented"
else
    fail_test "ccache caching is missing"
fi

if grep -q "actions/cache@v4" .github/workflows/ci.yml; then
    pass_test "GitHub Actions caching is implemented"
else
    fail_test "GitHub Actions caching is missing"
fi
echo ""

# Test 6: Check security scanning
echo "Test 6: Security Scanning Check"
if grep -q "trivy-action" .github/workflows/ci.yml; then
    pass_test "Trivy vulnerability scanning is implemented"
else
    fail_test "Trivy vulnerability scanning is missing"
fi

if grep -q "upload-sarif" .github/workflows/ci.yml; then
    pass_test "SARIF upload is implemented"
else
    fail_test "SARIF upload is missing"
fi

if grep -q "SBOM" .github/workflows/ci.yml; then
    pass_test "SBOM generation is implemented"
else
    warn_test "SBOM generation might be missing"
fi
echo ""

# Test 7: Check skip flags
echo "Test 7: Skip Flags Check"
SKIP_FLAGS=("skip-format" "skip-tidy" "skip-hook" "skip-build" "skip-coverage" "skip-docker" "skip-demo" "ci skip")

for flag in "${SKIP_FLAGS[@]}"; do
    if grep -q "\[$flag\]" .github/workflows/ci.yml; then
        pass_test "Skip flag '[$flag]' is implemented"
    else
        warn_test "Skip flag '[$flag]' might be missing"
    fi
done
echo ""

# Test 8: Check permissions
echo "Test 8: GitHub Permissions Check"
PERMISSIONS=("contents: read" "checks: write" "pull-requests: write" "security-events: write" "packages: write")

for perm in "${PERMISSIONS[@]}"; do
    if grep -q "$perm" .github/workflows/ci.yml; then
        pass_test "Permission '$perm' is set"
    else
        warn_test "Permission '$perm' might be missing"
    fi
done
echo ""

# Test 9: Check artifact retention
echo "Test 9: Artifact Retention Check"
if grep -q "retention-days: 7" .github/workflows/ci.yml; then
    pass_test "Docker image retention is set to 7 days"
else
    warn_test "Docker image retention might not be optimal"
fi

if grep -q "retention-days: 90" .github/workflows/ci.yml; then
    pass_test "SBOM/build info retention is set to 90 days"
else
    warn_test "SBOM/build info retention might not be optimal"
fi
echo ""

# Test 10: Check timeout settings
echo "Test 10: Timeout Settings Check"
if grep -q "timeout-minutes:" .github/workflows/ci.yml; then
    pass_test "Timeout settings are implemented"
else
    fail_test "Timeout settings are missing"
fi
echo ""

# Test 11: Check for proper error handling
echo "Test 11: Error Handling Check"
if grep -q "continue-on-error: true" .github/workflows/ci.yml; then
    pass_test "Non-blocking jobs have continue-on-error"
else
    warn_test "Some jobs might benefit from continue-on-error"
fi

if grep -q "if: always()" .github/workflows/ci.yml; then
    pass_test "Jobs use always() for proper error handling"
else
    warn_test "Some jobs might need always() condition"
fi
echo ""

# Test 12: Check workflow dispatch
echo "Test 12: Workflow Dispatch Check"
if grep -q "workflow_dispatch:" .github/workflows/ci.yml; then
    pass_test "Workflow dispatch is implemented"
else
    fail_test "Workflow dispatch is missing"
fi

if grep -q "run_all_jobs" .github/workflows/ci.yml; then
    pass_test "Manual override input is implemented"
else
    warn_test "Manual override input might be missing"
fi
echo ""

# Test 13: Validate cache key patterns
echo "Test 13: Cache Key Patterns Check"
if grep -q "hashFiles.*CMakeLists.txt" .github/workflows/ci.yml; then
    pass_test "Cache keys use CMakeLists.txt for hashing"
else
    warn_test "Cache keys might not be optimal for C++ projects"
fi

# Check that we're not using non-existent lock files
if grep -q "package-lock.json\|Cargo.lock\|go.sum" .github/workflows/ci.yml; then
    fail_test "Cache keys reference non-existent lock files for this C++ project"
else
    pass_test "Cache keys don't reference non-existent lock files"
fi
echo ""

# Test 14: Check for proper job orchestration
echo "Test 14: Job Orchestration Check"
if grep -q "dorny/paths-filter" .github/workflows/ci.yml; then
    pass_test "Change detection is implemented"
else
    fail_test "Change detection is missing"
fi

if grep -q "outputs:" .github/workflows/ci.yml; then
    pass_test "Job outputs are defined"
else
    fail_test "Job outputs are missing"
fi
echo ""

# Test 15: Check CI summary job
echo "Test 15: CI Summary Job Check"
if grep -A 10 "ci_summary:" .github/workflows/ci.yml | grep -q "GITHUB_STEP_SUMMARY"; then
    pass_test "CI summary uses GitHub Actions summary"
else
    fail_test "CI summary doesn't use GitHub Actions summary"
fi

if grep -A 10 "ci_summary:" .github/workflows/ci.yml | grep -q "if: always()"; then
    pass_test "CI summary has always() condition"
else
    fail_test "CI summary should have always() condition"
fi
echo ""

# Final summary
echo "========================================="
echo "Test Results Summary"
echo "========================================="
echo -e "${GREEN}Tests Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Tests Failed: $TESTS_FAILED${NC}"
echo -e "${YELLOW}Warnings: $(($TESTS_PASSED + $TESTS_FAILED - $TESTS_PASSED - $TESTS_FAILED))${NC}"
echo "========================================="

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ All critical tests passed!${NC}"
    echo "The CI workflow is ready for deployment."
    exit 0
else
    echo -e "${RED}❌ Some tests failed. Please review the issues above.${NC}"
    exit 1
fi
