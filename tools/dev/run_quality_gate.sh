#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
COVERAGE=0

if [[ "${1:-}" == "--coverage" ]]; then
    COVERAGE=1
fi

cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSAFETY_CORE_ENABLE_SANITIZERS=ON
cmake --build "$ROOT/build"
ctest --test-dir "$ROOT/build" --output-on-failure

bash "$ROOT/tools/safety/verify_policy.sh"

if [[ "$COVERAGE" -eq 1 ]]; then
    if ! command -v gcovr >/dev/null 2>&1; then
        echo "[WARN] gcovr is not installed; skipping coverage gate"
        exit 0
    fi

    cmake -S "$ROOT" -B "$ROOT/build-coverage" -DCMAKE_BUILD_TYPE=Debug -DSAFETY_CORE_ENABLE_SANITIZERS=OFF -DSAFETY_CORE_ENABLE_COVERAGE=ON
    cmake --build "$ROOT/build-coverage"
    ctest --test-dir "$ROOT/build-coverage" --output-on-failure
    gcovr --root "$ROOT" --filter "$ROOT/src" --filter "$ROOT/include" --exclude "$ROOT/build-coverage" \
        --print-summary --fail-under-line 80 --fail-under-branch 55
fi

echo "[OK] quality gate passed"
