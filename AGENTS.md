# Agent Guidelines

**This file contains guidelines and learned information for AI agents working on this repository.**

---

## Table of Contents

- [Development Workflow](#development-workflow)
- [Repository-Specific Information](#repository-specific-information)
- [Common Issues](#common-issues)
- [Agent Parallelization Guidelines](#agent-parallelization-guidelines)

---

## Development Workflow

### Always Build and Test Before Committing

**CRITICAL**: Before committing any changes, you must:

1. **Build the affected packages** in the Docker container:
   ```bash
   docker exec safety-autonomy-demo bash -c "cd /workspace/ros2_ws && colcon build --packages-select <package_name>"
   ```

2. **Test the changes** by actually running the affected functionality:
   - For launch file changes: Run the launch and verify it works
   - For node changes: Run the node and verify behavior
   - For script changes: Execute the script and verify output
   - For config changes: Launch with new config and verify

3. **Verify no regressions** in related functionality

### Example Workflow

```bash
# After making changes to safety_core_bringup
docker cp ros2/src/safety_core_bringup/launch/my_launch.py safety-autonomy-demo:/workspace/ros2_ws/src/safety_core_bringup/launch/
docker exec safety-autonomy-demo bash -c "cd /workspace/ros2_ws && colcon build --packages-select safety_core_bringup"
# Test the launch file
docker exec safety-autonomy-demo bash -c "source install/setup.bash && ros2 launch safety_core_bringup my_launch.py"
```

---

## Repository-Specific Information

### Docker Container

| Setting | Value |
|---------|-------|
| **Container name** | `safety-autonomy-demo` |
| **Workspace path** | `/workspace/ros2_ws` |
| **Source mount** | `/workspace/safety-autonomy-core` |

### Build Commands

| Command | Description |
|---------|-------------|
| `colcon build --packages-select <package>` | Build specific package |
| `colcon build` | Build all packages |

### Testing Commands

| Command | Description |
|---------|-------------|
| `source install/setup.bash` | Source workspace |
| `ros2 launch <package> <launch_file>` | Launch files |
| `ros2 run <package> <executable>` | Run nodes |
| `colcon test --packages-select safety_core_test` | Run unit/integration/component tests |
| `colcon test-result --verbose` | View detailed test results |

### Test Infrastructure

The `safety_core_test` package provides comprehensive testing:
- **Unit tests**: `time_utils`, `math_utils`, ROS bridge adapters
- **Component tests**: `sensor_monitor_node` (ROS2 topic-based)
- **Integration tests**: `safety_stack` (end-to-end safety system)
- **Coverage**: `ENABLE_COVERAGE=ON` CMake option for lcov/genhtml reports

All tests run in Docker via CI (Stage 5: `ros2_test` job).

### Code Style

- C++ files: Must pass `clang-format` (auto-applied by pre-commit hook)
- Pre-commit hook uses Docker container's clang-format if not available on host
- Configure git hooks: `git config core.hooksPath .githooks`

### Static Analysis

- **cppcheck**: MISRA-like rules, blocking in CI (`.cppcheck-suppressions` for known issues)
- **clang-tidy**: Security-critical checks enabled, low-value false-positives disabled in `.clang-tidy`
- Both tools run in CI Stage 4 (`clang_tidy` job) and fail the build on any finding

### Commit Hygiene

**CRITICAL**: Always squash related commits to keep the commit tree as clean as possible.

**Before pushing changes:**
1. Review your commits with `git log --oneline`
2. Identify groups of related commits (e.g., multiple CI fixes, feature iterations, bug fixes)
3. Squash related commits into single, meaningful commits using:
   - Interactive rebase: `git rebase -i <base-commit>`
   - Or soft reset: `git reset --soft <base-commit>` then `git commit`
4. Use descriptive commit messages that explain the "why" not just the "what"

**Commit grouping guidelines:**
- CI/CD improvements → single commit
- Feature development (localization, teleop, Docker) → single commit per feature
- Bug fixes for the same issue → single commit
- Documentation updates → single commit
- Release commits → keep separate

**Example:**
```bash
# Squash the last 5 commits into one
git reset --soft HEAD~5
git commit -m "feat: Add comprehensive localization improvements"
```

---

## Common Issues

### Launch File Not Found

If you get "file not found in share directory" errors:
1. Copy the file to the container: `docker cp local/file container:/workspace/ros2_ws/src/package/path/`
2. Rebuild the package: `colcon build --packages-select <package>`
3. Verify installation: `ls install/<package>/share/<package>/`

### Merge Conflicts

When pulling changes that conflict with local work:
1. Resolve conflicts by keeping the properly formatted version
2. Run clang-format on resolved files
3. Test the changes
4. Commit with `SKIP_CLANG_TIDY=1` if clang-tidy is not available

### Package Build Failures

If a package fails to build in the container:
1. Check for missing dependencies
2. Verify CMakeLists.txt is correct
3. Check for Python import errors
4. Ensure all required files are copied to the container
5. **Check if safety_autonomy_core needs rebuild** - If you see errors about missing methods in safety_core library, rebuild safety_autonomy_core first:
   ```bash
   docker exec safety-autonomy-demo bash -c "cd /workspace/ros2_ws && source /opt/ros/jazzy/setup.bash && colcon build --packages-select safety_autonomy_core"
   ```
   Then rebuild the package that depends on it.

---

## Agent Parallelization Guidelines

### When to Use Parallel Subagents

Use the `run_subagent` tool to parallelize work when:

1. **Independent file operations**: Multiple files can be read/edited simultaneously without conflicts
2. **Exploratory searches**: Different parts of the codebase can be explored in parallel
3. **Multi-step independent tasks**: Tasks that don't depend on each other can run concurrently
4. **Cross-cutting concerns**: Different aspects of a problem can be investigated simultaneously

### Parallelization Strategy

For this repository, effective parallelization patterns include:

**Codebase Exploration:**
- Launch multiple subagents to explore different directories (src/, include/, ros2/, tests/)
- Parallelize dependency tracing across multiple packages
- Concurrent search for different patterns or functions

**Testing and Validation:**
- Run different test suites in parallel when possible
- Validate multiple launch files concurrently
- Parallelize integration tests across different scenarios

**Build and CI:**
- Investigate build failures in parallel across different packages
- Analyze CI logs from different jobs simultaneously
- Parallelize Docker image testing across different configurations

### Example Parallelization

```bash
# Explore multiple package structures in parallel
run_subagent "Explore safety_core_ros package structure" "subagent_explore"
run_subagent "Explore safety_core_bringup package structure" "subagent_explore"
run_subagent "Explore test structure and coverage" "subagent_explore"

# Analyze different aspects of a problem in parallel
run_subagent "Search for memory allocation patterns" "subagent_general"
run_subagent "Search for thread safety issues" "subagent_general"
run_subagent "Search for error handling patterns" "subagent_general"
```

### Parallelization Constraints

**Do NOT parallelize when:**
- Tasks have dependencies on each other's results
- File modifications would conflict (same files being edited)
- Resources are limited (Docker container, build artifacts)
- Sequential execution is required for correctness

**Safe parallelization:**
- Read-only operations across different files
- Independent writes to different files
- Separate Docker containers or environments
- Non-overlapping resource usage

### Repository-Specific Parallelization

**Docker Operations:**
- Only one Docker container operation at a time (safety-autonomy-demo)
- Parallelize Docker image building across different stages if using BuildKit
- CI jobs can run in parallel (as implemented in Phase 1 optimizations)

**ROS 2 Operations:**
- Multiple ROS 2 nodes can be investigated in parallel
- Different launch files can be validated concurrently
- Topic and service analysis can be parallelized

**Build Operations:**
- Package builds are generally sequential due to dependencies
- Build analysis can be parallelized across different packages
- Test execution can be parallelized when tests are independent
