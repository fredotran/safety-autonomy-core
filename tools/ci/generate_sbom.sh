#!/usr/bin/env bash
# Generate a CycloneDX/SPDX SBOM for the repository's declared dependencies.
#
# Two tiers:
#   1. If `syft` is available -> full SBOM (SPDX-JSON + CycloneDX-JSON).
#   2. Fallback -> a deterministic plain-text manifest of pinned tools and
#      their resolved versions (always produces an artifact).
set -euo pipefail

OUT_DIR="${1:-sbom}"
mkdir -p "${OUT_DIR}"

echo "[sbom] output dir: ${OUT_DIR}"

if command -v syft >/dev/null 2>&1; then
    echo "[sbom] syft found — generating SPDX + CycloneDX"
    syft dir:. \
        --output "spdx-json=${OUT_DIR}/sbom.spdx.json" \
        --output "cyclonedx-json=${OUT_DIR}/sbom.cdx.json"
else
    echo "[sbom] syft not found — writing deterministic manifest"
    {
        echo "# SBOM manifest (toolchain + declared deps)"
        echo "generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo "git: $(git rev-parse HEAD 2>/dev/null || echo n/a)"
        echo
        echo "## toolchain"
        for tool in cmake gcc g++ clang clang++ clang-format clang-tidy cppcheck gcovr python3; do
            if command -v "${tool}" >/dev/null 2>&1; then
                ver="$("${tool}" --version 2>/dev/null | head -1 || echo '?')"
                printf '%-14s %s\n' "${tool}" "${ver}"
            fi
        done
        echo
        echo "## declared third-party deps (CMake/package manifests)"
        grep -rhoP 'find_package\(\K[A-Za-z0-9_]+' CMakeLists.txt ros2/src/*/CMakeLists.txt 2>/dev/null | sort -u
    } > "${OUT_DIR}/manifest.txt"
    cat "${OUT_DIR}/manifest.txt"
fi

echo "[sbom] done"
