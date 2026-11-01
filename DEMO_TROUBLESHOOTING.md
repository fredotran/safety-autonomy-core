# Demo Troubleshooting

Common failures when running the ROS 2 demo, with diagnosis and fixes. For RViz
display problems specifically, see [ros2/RVIZ_FIX_GUIDE.md](ros2/RVIZ_FIX_GUIDE.md).

## Container fails to start: stale Xauthority mount

**Symptom**

```
error mounting "/run/user/1000/.mutter-Xwaylandauth.XXXXXX" to rootfs at
"/root/.Xauthority": ... not a directory
```

**Cause** — the compose file bind-mounts `${XAUTHORITY:-~/.Xauthority}` into the
container. The recorded Xauthority path (often a transient `/run/user/...` file)
no longer exists after reboot/re-login.

**Fix**

```bash
# Recreate the container with a valid (or empty) Xauthority file
touch ~/.Xauthority
docker rm safety-autonomy-demo
XAUTHORITY=~/.Xauthority docker compose up -d
```

If no GUI forwarding is needed, run a plain container instead:

```bash
docker run -d --name safety-test-ws --entrypoint bash \
    -v $(pwd):/workspace/safety-autonomy-core \
    safety-autonomy-core:latest -c "sleep infinity"
```

## "Launch file not found in share directory"

**Symptom** — `ros2 launch safety_core_bringup <file>` errors saying the file is
not in the package's share directory.

**Fix** — the workspace copy is stale relative to the mounted repo:

```bash
docker cp ros2/src/safety_core_bringup/launch/<file> \
    safety-test-ws:/workspace/ros2_ws/src/safety_core_bringup/launch/
docker exec safety-test-ws bash -c \
    "cd /workspace/ros2_ws && source /opt/ros/jazzy/setup.bash && \
     colcon build --packages-select safety_core_bringup"
```

## Package fails to build: missing methods in `safety_core`

**Symptom** — `safety_core_ros` or `safety_core_nav2` fail with errors about
missing methods/symbols in `safety_core::*`.

**Cause** — the installed `safety_autonomy_core` library predates your header
changes; dependent packages pick up the stale install space.

**Fix** — rebuild `safety_autonomy_core` first, then the dependent package:

```bash
docker exec safety-test-ws bash -c \
    "cd /workspace/ros2_ws && source /opt/ros/jazzy/setup.bash && \
     colcon build --packages-select safety_autonomy_core"
```

## Nodes launch but `/safety/*` topics are silent

- Check the nodes are alive: `ros2 node list`
- Check QoS compatibility: envelope/status topics use reliable+volatile QoS;
  `ros2 topic echo` defaults are compatible.
- Verify scan input: without `/scan` data the envelope node publishes nothing on
  some configurations. `ros2 topic hz /scan`
- Different `ROS_DOMAIN_ID` between shell and container hides all traffic.

## `safety/state` escalates to `LocalizationLost` / `SafeStop` right after launch

Expected fail-safe behavior when no odometry or scan data reaches the stack —
sensor streams that never arrive are stale by definition (measured from node
startup, after `startup_grace_period_s`):

- `mode: 5` (LocalizationLost) once `localization_timeout_s` elapses (0.5 s)
- `mode: 7` (SafeStop) + `fault_latched: true` once odometry age reaches
  2 × `localization_timeout_s`; `fault_code` shows `1002` (localization stale),
  then `0xE001` once the scan watchdog's EMERGENCY zone latches

Check the sensor inputs rather than the safety nodes:

- `ros2 topic hz /odom` and `ros2 topic hz /scan` — both must be publishing
- `safety_only.launch.py` expects an external source (bag replay or hardware);
  use `safety_sim.launch.py` to get both from Gazebo
- The supervisor reads `/odom`'s `header.stamp`: a driver that leaves the
  stamp at zero still reads as stale
- While odometry is stale the envelope is evaluated at
  `envelope.max_speed_mps` (conservative), so zones look more aggressive than
  the real situation

## Gazebo does not open / black window

Almost always X11 forwarding or GPU related — see
[ros2/RVIZ_FIX_GUIDE.md](ros2/RVIZ_FIX_GUIDE.md); the same fixes apply to Gazebo.
To rule out rendering entirely, run headless:

```bash
ros2 launch safety_core_bringup safety_sim.launch.py headless:=true
```

## Demo validation script fails in CI

`tools/ci/validate_demo.sh` leaves logs at `/tmp/safety_stack.log` and
`~/.ros/log/`. Download the `demo-logs` artifact from the workflow run; the most
common cause is the safety stack taking >10 s to come up on a cold runner —
`launch_safety_stack` waits only 10 s before declaring failure.

## clang-format / pre-commit hook issues

```bash
# Hook is not installed
git config core.hooksPath .githooks

# clang-format missing on host — the hook falls back to the container
docker exec safety-test-ws clang-format --version

# clang-tidy not configured (no compile_commands.json)
SKIP_CLANG_TIDY=1 git commit -m "..."
```
