# Certifiable Build Partition

## Purpose

Defines the boundary between the certifiable safety core and the integration
layer, and turns key safety claims into compiler-enforced evidence.

## Partition

| Unit | Contents | Certification scope |
|---|---|---|
| `safety_core` / `safety_core_certifiable` | `src/**/*.cpp` listed in `SAFETY_CORE_SOURCES` + `include/safety_core/` public headers | In scope |
| `safety_core_app` | demo/main entry point | Out of scope (demo harness) |
| `ros2/src/*` | ROS 2 nodes, messages, launch, Nav2 plugin | Out of scope (integration wrapper) |
| `tests/`, `fuzz/`, `benchmarks/` | verification assets | Verification evidence, not shipped |

## Enforced constraints

`safety_core_certifiable` compiles the **identical** source list as `safety_core`
under:

- `-fno-exceptions` — any `throw`/`try`/`catch` is a compile error. This makes
  the project's "noexcept everywhere" claim compiler-enforced rather than
  conventional.
- `-fno-rtti` — any `dynamic_cast`/`typeid` is a compile error.
- `-Wall -Wextra -Wpedantic -Werror` — stricter than the default target.

A regression that introduces exceptions or RTTI into the core **fails this
target**, giving an executable check on the certification boundary.

## Dependency surface

- **No ROS**: zero `rclcpp`/ament includes in the core source list.
- **No Eigen**: no source file includes Eigen headers. The former hard
  `find_package(Eigen3)` + `FATAL_ERROR` was a dead dependency and has been
  removed — the certifiable core now has **zero third-party dependencies**
  beyond the C++ standard library.

## Verification

```bash
cmake -S . -B build && cmake --build build --target safety_core_certifiable
```

CI job `certifiable_build` builds this target on every push so the partition
cannot silently rot.
