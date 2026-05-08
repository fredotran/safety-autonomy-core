# CI Tools

This directory contains CI/CD validation scripts and tools for the safety-autonomy-core project.

## Demo Validation Script

`validate_demo.sh` - Comprehensive demo validation script for CI/CD pipeline

### Usage

The script validates that the safety autonomy core demos are working correctly by:

1. Checking ROS 2 environment and workspace build
2. Validating safety nodes are running
3. Validating safety topics exist
4. Running demo scripts with timeout protection
5. Validating launch files

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

The script is integrated into the GitHub Actions CI pipeline in `.github/workflows/ci.yml`:

```yaml
demo_tests:
  name: demo_tests (comprehensive Docker demo validation)
  steps:
    - name: Run quick demo validation
      run: |
        docker compose run --rm safety-autonomy-demo bash -c "
          chmod +x /workspace/safety-autonomy-core/tools/ci/validate_demo.sh &&
          /workspace/safety-autonomy-core/tools/ci/validate_demo.sh quick
        "
```

### Validation Checks

The script performs the following validations:

1. **Environment Checks**
   - ROS 2 environment sourced
   - Workspace built successfully

2. **Node Validation**
   - safety_envelope_node
   - safety_supervisor_node
   - safety_drive_bridge_node

3. **Topic Validation**
   - /safety/state
   - /safety/envelope_status
   - /safety/safe_stop
   - /cmd_vel_nav

4. **Demo Execution**
   - Quick demo (2-minute validation)
   - Comprehensive demo (5-minute validation)
   - Launch file validation

### Error Handling

The script uses timeout protection to prevent hanging:
- Quick demo: 120 seconds
- Comprehensive demo: 300 seconds
- Launch validation: 30 seconds

Non-critical failures are reported as warnings and don't cause the script to fail.

### Logs and Artifacts

Demo logs are automatically uploaded as CI artifacts for debugging:
- ROS 2 logs: `~/.ros/log/`
- Temporary logs: `/tmp/`
