# Agent Guidelines

This file contains guidelines and learned information for AI agents working on this repository.

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

## Repository-Specific Information

### Docker Container
- Container name: `safety-autonomy-demo`
- Workspace path: `/workspace/ros2_ws`
- Source mount: `/workspace/safety-autonomy-core`

### Build Commands
- Build specific package: `colcon build --packages-select <package>`
- Build all packages: `colcon build`

### Testing Commands
- Source workspace: `source install/setup.bash`
- Launch files: `ros2 launch <package> <launch_file>`
- Run nodes: `ros2 run <package> <executable>`

### Code Style
- C++ files: Must pass `clang-format` (auto-applied by pre-commit hook)
- Pre-commit hook uses Docker container's clang-format if not available on host
- Configure git hooks: `git config core.hooksPath .githooks`

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
