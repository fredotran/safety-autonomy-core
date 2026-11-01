# Dependency Pinning & Reproducibility

## Policy

The certifiable core (`safety_core`) has **zero third-party dependencies**
beyond the C++ standard library. Everything else is a *build-time* or
*integration* dependency, pinned per layer:

| Layer | Mechanism |
|---|---|
| Host toolchain (CI) | apt packages pinned by the ubuntu-24.04 runner image; resolved versions emitted into the SBOM manifest |
| ROS 2 | Jazzy (Dockerfile base image tag, not `latest`) |
| Fuzzing | clang-18 (explicit version in CI install step) |
| SBOM | syft pinned by `SYFT_VERSION` env in CI |
| Python test deps | stdlib-only (`pyyaml` system package) |

## Adding a dependency

1. Prefer standard library or vendored-in-repo over a new package.
2. If unavoidable: pin a concrete version (no `latest`/floating `>=`),
   add it to this table, and let the SBOM job capture the resolved version.
3. **Never** add a dependency to `SAFETY_CORE_SOURCES`/the certifiable target
   without updating `certifiable_partition.md`.

## Regenerating the SBOM

```bash
tools/ci/generate_sbom.sh sbom/   # syft if present, else text manifest
```
