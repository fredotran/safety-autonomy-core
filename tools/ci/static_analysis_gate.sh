#!/usr/bin/env bash
# Static analysis ratchet gate.
#
# Runs cppcheck and clang-tidy over src/ + include/ and fails only on *new*
# findings relative to a checked-in baseline. The baseline records, per
# (file, check) pair, how many findings are currently tolerated. Fixing
# findings never fails the gate; introducing new ones does.
#
# Usage:
#   tools/ci/static_analysis_gate.sh [--build-dir DIR] [--update]
#
#   --build-dir DIR  CMake build dir containing compile_commands.json
#                    (default: build; configured automatically if missing)
#   --update         Regenerate the baseline from the current findings
#
# Baseline: tools/ci/baselines/static_analysis_baseline.txt
# Regenerate after a cleanup pass: ./tools/ci/static_analysis_gate.sh --update

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build}"
BASELINE="${REPO_ROOT}/tools/ci/baselines/static_analysis_baseline.txt"
UPDATE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --update)    UPDATE=1; shift ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

cd "${REPO_ROOT}"

# compile_commands.json is required by clang-tidy -p
if [[ ! -f "${BUILD_DIR}/compile_commands.json" ]]; then
    echo "[gate] Configuring ${BUILD_DIR} for compile_commands.json"
    cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DSAFETY_CORE_ENABLE_SANITIZERS=OFF >/dev/null
fi

TIDY_SOURCES=$(find src -name '*.cpp' | sort)
CURRENT=$(mktemp)
trap 'rm -f "${CURRENT}"' EXIT

# --- cppcheck: normalize to 'file|check' ----------------------------------
echo "[gate] Running cppcheck"
cppcheck --enable=all \
         --inline-suppr \
         --std=c++20 \
         --suppress=missingIncludeSystem \
         --suppress=unusedFunction \
         --suppressions-list=.cppcheck-suppressions \
         --template='{file}|{id}' \
         -I include \
         -I /usr/include/eigen3 \
         src/ include/ 2>&1 \
    | grep -E '^[^ ]+\.(cpp|hpp|h)\|' >> "${CURRENT}" || true

# --- clang-tidy: normalize 'file:line:col: warning: msg [check]' ----------
# Paths are relativized to the repo root so the baseline is portable across
# checkouts (CI runners compile from a different absolute path).
echo "[gate] Running clang-tidy"
# shellcheck disable=SC2086
clang-tidy -p "${BUILD_DIR}" ${TIDY_SOURCES} 2>/dev/null \
    | grep -oE '^[^ ]+\.(cpp|hpp|h):[0-9]+:[0-9]+: (warning|error): .*\[[^]]+\]' \
    | sed -E "s#^${REPO_ROOT}/##" \
    | sed -E 's#^([^:]+):[0-9]+:[0-9]+: (warning|error): .*\[([^]]+)\]$#\1|\3#' \
    | sed -E 's#(,[^|]*)?$##' \
    >> "${CURRENT}" || true

# --- aggregate to 'file|check|count' --------------------------------------
SORTED=$(mktemp)
trap 'rm -f "${CURRENT}" "${SORTED}"' EXIT
sort "${CURRENT}" | uniq -c | awk '{print $2"|"$1}' | sort > "${SORTED}"

if [[ ${UPDATE} -eq 1 ]]; then
    mkdir -p "$(dirname "${BASELINE}")"
    cp "${SORTED}" "${BASELINE}"
    echo "[gate] Baseline updated: $(wc -l < "${BASELINE}") tolerated findings"
    exit 0
fi

if [[ ! -f "${BASELINE}" ]]; then
    echo "[gate] No baseline found at ${BASELINE}"
    echo "[gate] Run: tools/ci/static_analysis_gate.sh --update"
    exit 2
fi

# --- compare: fail on findings exceeding baseline counts -------------------
NEW_FINDINGS=0
while IFS='|' read -r file check count; do
    base_count=$(awk -F'|' -v f="${file}" -v c="${check}" \
        '$1 == f && $2 == c {print $3}' "${BASELINE}")
    base_count=${base_count:-0}
    if [[ ${count} -gt ${base_count} ]]; then
        echo "[gate] NEW: ${file}|${check} (${base_count} tolerated, ${count} found)"
        NEW_FINDINGS=$((NEW_FINDINGS + count - base_count))
    fi
done < "${SORTED}"

FIXED=$(comm -23 <(cut -d'|' -f1,2 "${BASELINE}" | sort) <(cut -d'|' -f1,2 "${SORTED}" | sort) | wc -l)
if [[ ${FIXED} -gt 0 ]]; then
    echo "[gate] ${FIXED} baseline entries fully resolved — consider --update"
fi

if [[ ${NEW_FINDINGS} -gt 0 ]]; then
    echo "[gate] FAIL: ${NEW_FINDINGS} new finding(s) above baseline"
    exit 1
fi

echo "[gate] PASS: no new findings ($(wc -l < "${SORTED}") tolerated)"
