# Demo Guide

Complete documentation for the safety-autonomy-core ROS 2 demo system. For setup and
prerequisites see [README.md](README.md) and [markdown/docs/DOCKER.md](markdown/docs/DOCKER.md).
When something goes wrong, see [DEMO_TROUBLESHOOTING.md](DEMO_TROUBLESHOOTING.md).

## Starting a demo

```bash
./setup_demo.sh
```

The script checks Docker availability, builds the image if needed, then presents an
interactive menu. Pass `--local` to run against a local ROS 2 Jazzy install instead.

## Demo modes

### Quick Demo (1-2 min)

Fast validation of the core safety features: zone transitions, mode escalation and
safe-stop assertion. Suitable for smoke-testing a build.

```bash
tools/ci/validate_demo.sh quick          # inside the container
ros2 launch safety_core_bringup safety_sim.launch.py
```

### Comprehensive Demo (5-7 min)

Walks through every safety capability: CLEAR/WARNING/PROTECTIVE/EMERGENCY zone
transitions, Degraded mode, localization loss, fault latching and clear-fault
recovery. See [ros2/COMPREHENSIVE_DEMO.md](ros2/COMPREHENSIVE_DEMO.md) for the
scenario-by-scenario breakdown.

```bash
ros2 launch safety_core_bringup comprehensive_demo.launch.py
```

### Interactive Demo

Menu-driven on-demand scenario testing (moving obstacles, sensor failures,
teleoperation). Use `teleop_node` to drive the AGV manually:

```bash
ros2 run safety_core_ros teleop_node
```

### Full warehouse demo

The complete stack: Gazebo Harmonic warehouse, Nav2, SLAM Toolbox, the safety nodes
and RViz visualization.

```bash
ros2 launch safety_core_bringup agv_warehouse.launch.py
```

### Safety nodes only (no Gazebo)

For bag replay or hardware-in-the-loop testing:

```bash
ros2 launch safety_core_bringup safety_only.launch.py
```

### Diagnostics mode

Launches the stack with the diagnostics monitor focused on `DiagnosticEvent`
traffic and health beacons:

```bash
ros2 launch safety_core_bringup diagnostics.launch.py
```

## What to observe

| Signal | How to check |
|--------|--------------|
| Zone transitions | `ros2 topic echo /safety/envelope_status` |
| Mode changes | `ros2 topic echo /safety/state` (`mode` field: 0=Init ... 7=SafeStop) |
| Safe-stop assertion | `ros2 topic echo /safety/safe_stop` |
| Diagnostic events | `ros2 topic echo /safety/diagnostics` |
| Zone visualization | RViz markers on `/safety/zone_markers` |

Healthy baseline (no obstacles): `mode: 1` (Idle/Moving), `zone.zone: 0` (CLEAR),
`fault_latched: false`, `safe_stop: false`.

## Environment variables

| Variable | Purpose | Default |
|----------|---------|---------|
| `ROS_DOMAIN_ID` | DDS domain (isolate demo traffic) | `0` |
| `DISPLAY` | X11 display for Gazebo/RViz | `:0` |
| `XAUTHORITY` | X11 auth file mounted in container | `~/.Xauthority` |
| `GZ_SIM_RESOURCE_PATH` | Extra model paths (e.g. AWS warehouse) | unset |

## Tips

- Run `ros2 node list` after launch — you should see the three safety nodes plus
  sim/nav nodes depending on the mode.
- The envelope node ignores returns closer than `scan_ignore_min_range_m`
  (default 0.5 m) so the AGV's own footprint does not trigger protective stops.
- If only the safety stack is wanted in CI, `tools/ci/validate_demo.sh quick`
  performs the same checks the `demo_tests` workflow job runs.
