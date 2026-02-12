# Safety Autonomy Core

High-assurance C++ core for safety-minded robotics and autonomous vehicles: deterministic state machines, bounded executors, diagnostics, and motion/control primitives with MISRA/AUTOSAR-inspired rules. Focused on AGV/AMR operation inside dynamic warehouses (pedestrians, forklifts, pop-up obstacles, temporary localization loss).

## Scope
- Deterministic mode/state management (Init, Standby, Active, Degraded, ObstacleHold, LocalizationLost, SafeStop)
- Diagnostics and health state with latched faults
- Time/budget utilities and bounded task executor
- Safety envelope helper for stop-distance checks against dynamic obstacles
- Foundations for filters, planners, and controllers (to be added iteratively)

## Repository layout
- `include/safety_core/` public headers
- `src/` implementations (kept minimal; most logic header-only for determinism)
- `tests/` lightweight unit/property tests (state machine, safety envelope)
- `CMakeLists.txt` library + smoke test target
- `.clang-format`, `.clang-tidy` house style and lint rules
- `.gitlab-ci.yml` CI pipeline (format, clang-tidy, build, tests, security scans)

## Build and test (local)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSAFETY_CORE_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Notes:
- Sanitizers are enabled by default for non-safety builds; disable with `-DSAFETY_CORE_ENABLE_SANITIZERS=OFF` when targeting production-like safety builds.
- Warnings are treated as errors by default; toggle with `-DSAFETY_CORE_ENABLE_WERROR=OFF` if bootstrapping a new toolchain.

## Coding standards (starter set)
- MISRA/AUTOSAR-inspired: no dynamic allocation in RT paths, no exceptions/RTTI in core, fixed-capacity containers, explicit initialization.
- Strong typing and units/frames; [[nodiscard]] for status-returning APIs.
- Saturation/NaN/Inf guards at boundaries; deterministic math (no fast-math).
- Concurrency: bounded queues only; timed waits; avoid unbounded locks.

## CI pipeline
- `format_check`: clang-format guard on `include/`, `src/`, `tests/`
- `clang_tidy`: static analysis over library sources
- `build_and_test`: CMake build + ctest (with sanitizers on by default)
- Security templates: GitLab SAST + Secret Detection

## Roadmap (next steps)
- Add numerics layer (bounded EKF/complementary filters) with invariants and property tests
- Add motion primitives and control layer (PID with anti-windup, safety envelopes)
- Introduce diagnostics transport and health beacons
- Provide safety case outline (hazards, mitigations, traceability)

## License
TBD (select per-robot/per-project commercial licensing with mandatory support/maintenance).
