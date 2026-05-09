# CI Tools

This directory contains CI/CD validation scripts and tools for the safety-autonomy-core project.

## Demo Validation Script

`validate_demo.sh` - Comprehensive demo validation script for CI/CD pipeline

### Usage

The script validates that the safety autonomy core demos are working correctly by:

1. Checking ROS 2 environment and workspace build
2. Launching the safety stack (required infrastructure)
3. Validating safety nodes are running
4. Validating safety topics exist
5. Running demo scripts with timeout protection
6. Validating launch files
7. Proper error detection and reporting (failures cause CI to fail)

### Running Locally

```bash
# Inside the Docker container
cd /workspace/safety-autonomy-core
./tools/ci/validate_demo.sh [quick|comprehensive|launch <file>|all]
```

### Examples

```bash
# Validate quick demo only
./tools/ci/validate_demo.sh quick

# Validate comprehensive demo only
./tools/ci/validate_demo.sh comprehensive

# Validate specific launch file
./tools/ci/validate_demo.sh launch safety_core_bringup safety_sim.launch.py

# Run all validations
./tools/ci/validate_demo.sh all
```

### CI Integration

The script is integrated into the GitHub Actions CI pipeline in `.github/workflows/ci.yml` with Phase 1 optimizations:

**Phase 1 Optimizations:**
- **Parallel Execution**: Demo tests run in parallel using matrix strategy (quick, comprehensive, launch_safety, launch_diagnostic)
- **Smart Test Selection**: Demo tests only run when Docker-related files change (docker-compose.yml, Dockerfile, CI workflow, ros2/ directory)
- **Docker BuildKit Caching**: Layer caching via GitHub Actions cache for faster image builds
- **Optimized Timeouts**: Reduced timeouts for faster feedback (quick: 90s, comprehensive: 240s, launch: 20s)

```yaml
demo_tests:
  name: demo_tests (comprehensive Docker demo validation)
  strategy:
    matrix:
      demo_type: [quick, comprehensive, launch_safety, launch_diagnostic]
  steps:
    - name: Run ${{ matrix.demo_type }} demo validation
      if: steps.check_changes.outputs.docker_changed == 'true'
      run: |
        docker run --rm --network host safety-autonomy-core:latest bash -c "
          chmod +x /workspace/safety-autonomy-core/tools/ci/validate_demo.sh &&
          /workspace/safety-autonomy-core/tools/ci/validate_demo.sh ${{ matrix.demo_type }}
        "
```

### Validation Checks

The script performs the following validations:

1. **Environment Checks**
   - ROS 2 environment sourced
   - Workspace built successfully

2. **Safety Stack Launch**
   - Automatically launches safety_sim.launch.py before running demos
   - Validates launch success
   - Proper cleanup on exit

3. **Node Validation**
   - safety_envelope_node
   - safety_supervisor_node
   - safety_drive_bridge_node
   - Requires at least one node to be running

4. **Topic Validation**
   - /safety/state
   - /safety/envelope_status
   - /safety/safe_stop
   - /cmd_vel_nav
   - Requires at least one topic to exist

5. **Demo Execution**
   - Quick demo (90-second validation)
   - Comprehensive demo (4-minute validation)
   - Launch file validation

### Error Handling

The script uses proper error detection and reporting:

**Timeout Protection:**
- Quick demo: 90 seconds (optimized from 120s)
- Comprehensive demo: 240 seconds (optimized from 300s)
- Launch validation: 20 seconds (optimized from 30s)

**Error Detection:**
- Exit code 0: Success
- Exit code 124: Timeout (reported as failure)
- Other exit codes: Failure with log output

**Failure Behavior:**
- All failures cause the script to exit with error code 1
- Failed validations print log output for debugging
- Safety stack is properly cleaned up on failure
- No error masking - failures propagate to CI

**Automatic Cleanup:**
- Safety stack is stopped on script exit (trap handler)
- Temporary processes are killed
- PID files are cleaned up

### Logs and Artifacts

Demo logs are automatically uploaded as CI artifacts for debugging:
- ROS 2 logs: `~/.ros/log/`
- Temporary logs: `/tmp/`
- Demo-specific logs: `/tmp/quick_demo.log`, `/tmp/comprehensive_demo.log`
- Safety stack logs: `/tmp/safety_stack.log`

### Architecture

The validation script follows this flow for demo validation:

1. **Environment Setup** - Check ROS 2 and workspace
2. **Safety Stack Launch** - Start required infrastructure
3. **Infrastructure Validation** - Verify nodes and topics
4. **Demo Execution** - Run demo with timeout
5. **Safety Stack Cleanup** - Stop infrastructure
6. **Result Reporting** - Success or failure with logs

For launch file validation:
1. **Launch File Execution** - Start launch file
2. **Infrastructure Validation** - Verify nodes and topics
3. **Cleanup** - Stop launch file
4. **Result Reporting** - Success or failure with logs
