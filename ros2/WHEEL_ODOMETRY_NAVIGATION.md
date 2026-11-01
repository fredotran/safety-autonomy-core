# Wheel Odometry Navigation

Configuration guide for running Nav2 on wheel odometry only — no static map, no
AMCL, no SLAM. This is the default mode of `agv_warehouse.launch.py`
(`environment:=outdoor`).

## When to use it

- Quick bring-up and safety-stack testing without a localization pipeline.
- Environments where no pre-built map exists and SLAM is undesired.
- Hardware sanity checks (does the robot drive? do safety stops trigger?).

Do **not** use it for missions that need accurate global positioning: wheel
odometry drifts unboundedly, so goals more than a few meters away accumulate
error. Use [LOCALIZATION.md](LOCALIZATION.md) (EKF fusion) or SLAM mode instead.

## How it works

`config/nav2_params_odometry.yaml` differs from `nav2_params.yaml` in five ways:

| Aspect | `nav2_params_odometry.yaml` | `nav2_params.yaml` |
|--------|-----------------------------|---------------------|
| Global frame | `odom` (drifts) | `map` (fixed) |
| Global costmap | `rolling_window: true`, no `static_layer` | static map layer |
| AMCL | removed | present |
| map_server / map_saver | removed | present |
| `bt_navigator.global_frame` | `odom` | `map` |

Both costmaps live in the `odom` frame, so Nav2 plans against a rolling local
window built from live sensor data — no `map -> odom` TF is required.

## Running

```bash
# Default: wheel-odometry mode (environment:=outdoor is the default)
ros2 launch safety_core_bringup agv_warehouse.launch.py

# Explicit
ros2 launch safety_core_bringup agv_warehouse.launch.py environment:=outdoor

# Contrast: SLAM Toolbox mode (publishes map -> odom)
ros2 launch safety_core_bringup agv_warehouse.launch.py environment:=indoor
```

## Prerequisites

- `/odom` topic + `odom -> base_link` TF. In simulation these come from the
  Gazebo diff-drive plugin; on hardware, from your odometry node or the local
  EKF (`ekf_config.yaml` first stage).
- Laser scan (or point cloud) for the rolling costmap obstacle layer.
- The `IsSafe` BT condition (`safety_core_nav2`) is in `plugin_lib_names`, so
  the tree still gates motion on `/safety/state` — safe stops work exactly as in
  map mode.

## Gotchas

- **"Goal is outside the rolling window"** — expected: keep goals within the
  rolling window size, or raise `global_costmap.rolling_window` width/height.
- **Drift on long runs** — wheel slip accumulates; watchdog `sensor_timeout`
  (0.2 s in `ekf_config.yaml`) flags stale odometry and the supervisor can
  escalate to `LOCALIZATION_LOST`. That is by design.
- **RViz Fixed Frame** — use `odom`, not `map` (no `map` frame exists in this
  mode). See [RVIZ_FIX_GUIDE.md](RVIZ_FIX_GUIDE.md).
