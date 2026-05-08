# safety-autonomy-core ROS 2 wrapper + AGV warehouse simulation

ROS 2 Jazzy + Gazebo Harmonic + Nav2 integration for the [`safety_autonomy_core`](../) C++ library, including a self-contained industrial warehouse simulation with a differential-drive AGV.

## What this provides

| Package                  | Type        | Purpose                                                                                |
|--------------------------|-------------|----------------------------------------------------------------------------------------|
| `safety_core_msgs`       | rosidl      | Custom messages (`SafetyState`, `SafetyZone`, `EnvelopeStatus`, `DiagnosticEvent`, `MonitorEvent`) |
| `safety_core_ros`        | C++ nodes   | Wrapper nodes integrating safety_core into rclcpp                                      |
| `safety_core_nav2`       | C++ plugin  | Nav2 behavior-tree plugin (`IsSafe` condition node)                                    |
| `safety_core_sim`        | resources   | AGV URDF (xacro), industrial warehouse SDF, `ros_gz_bridge` config, sim launch         |
| `safety_core_bringup`    | resources   | Top-level launch files, Nav2 params, SLAM Toolbox params, RViz config                  |
| `safety_autonomy_core`   | symlink     | The C++ library at the repo root, surfaced as an ament package                         |

## Architecture

```text
┌────────────────────────── Gazebo Harmonic ──────────────────────────┐
│ industrial_warehouse.sdf + AGV (diff drive, 2D LiDAR, IMU)          │
└─────────────────────────────────────────────────────────────────────┘
        │  /scan, /imu, /odom, /clock          ▲  /cmd_vel
        ▼                                       │
┌─────────────────────── ros_gz_bridge ──────────────────────────────┐
│ Bidirectional bridge (config: ros_gz_bridge.yaml)                  │
└────────────────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────── safety_envelope_node ───────────────────────────┐
│  Reads /scan in robot footprint corridor                            │
│  Calls safety_core::safety::evaluate_stop_distance(footprint)       │
│  Publishes:                                                         │
│   - /safety/envelope_status (zone classification + recommended v)   │
│   - /safety/zone_markers    (RViz visualization of zones)           │
└────────────────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────── safety_supervisor_node ─────────────────────────┐
│  Owns safety_core::ModeStateMachine + SafetySupervisor              │
│  Drives mode transitions on zone changes:                           │
│   Clear/Warning  → Moving                                           │
│   Protective     → AvoidingObstacle                                 │
│   Emergency      → SafeStop + latch_fault(0xE001)                   │
│  Tracks localization staleness via /odom timestamps                 │
│  Publishes:                                                         │
│   - /safety/state       (mode, zone, fault flags)                   │
│   - /safety/safe_stop   (boolean for downstream gating)             │
│   - /safety/diagnostics (DiagnosticEvent stream from safety_core)   │
└────────────────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────── Nav2 stack ─────────────────────────────────────┐
│  bt_navigator + IsSafe BT condition (safety_core_nav2 plugin)       │
│  controller_server (MPPI) → velocity_smoother → /cmd_vel_smoothed   │
│  collision_monitor (zone polygons)         → /cmd_vel_nav           │
└────────────────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────── safety_drive_bridge_node ───────────────────────┐
│  Subscribes /cmd_vel_nav, /safety/envelope_status, /safety/safe_stop│
│  Applies safety envelope:                                           │
│   - Clamp v to recommended_speed_limit_mps                          │
│   - Stale-command watchdog (cmd_freshness_s)                        │
│   - On safe_stop: jerk-limited deceleration profile                 │
│   - On fault_latched: zero twist                                    │
│  Publishes /cmd_vel → Gazebo                                        │
└────────────────────────────────────────────────────────────────────┘
```

## System requirements

- **Ubuntu 24.04** (Noble)
- **ROS 2 Jazzy** (`apt install ros-jazzy-desktop`)
- **Gazebo Harmonic** (`apt install gz-harmonic`)
- **`ros_gz` packages** (`apt install ros-jazzy-ros-gz`)
- **Optional, for full Nav2 integration**:
  ```bash
  sudo apt install \
    ros-jazzy-navigation2 \
    ros-jazzy-nav2-bringup \
    ros-jazzy-slam-toolbox \
    ros-jazzy-behaviortree-cpp-v3
  ```
- The C++ library at the repo root (built automatically by colcon).

## Build

```bash
cd safety-autonomy-core/ros2
source /opt/ros/jazzy/setup.bash

# Default build skips the optional Nav2 BT plugin if Nav2 is not installed.
colcon build \
    --packages-skip safety_core_nav2 \
    --cmake-args \
        -DSAFETY_CORE_ENABLE_AMENT=ON \
        -DSAFETY_CORE_ENABLE_SANITIZERS=OFF \
        -DSAFETY_CORE_ENABLE_WERROR=OFF
```

If Nav2 is installed, drop `--packages-skip safety_core_nav2` to also build the BT plugin.

> **Note**: Sanitizers must be disabled when building under colcon because the safety_core library propagates `-fsanitize=address,undefined` as PUBLIC link options, which conflicts with rclcpp's pre-built libraries.

## Run

### Full demo (Gazebo + safety_core + Nav2 + SLAM + RViz)

```bash
source install/setup.bash
ros2 launch safety_core_bringup agv_warehouse.launch.py
```

In RViz, use the **Nav2 Goal** tool (toolbar) to send a navigation goal. The AGV will plan and execute, stopping automatically when the safety envelope demands.

Optional launch arguments:

| Argument        | Default | Description                                  |
|-----------------|---------|----------------------------------------------|
| `use_sim_time`  | `true`  | Use Gazebo's `/clock`                        |
| `rviz`          | `true`  | Launch RViz with the demo config             |
| `slam`          | `true`  | Launch SLAM Toolbox (online async mapping)   |
| `nav2`          | `true`  | Launch Nav2 stack (only if installed)        |

Example with no Nav2:

```bash
ros2 launch safety_core_bringup agv_warehouse.launch.py nav2:=false
```

You can drive manually (without Nav2) by publishing to `/cmd_vel_nav`:

```bash
ros2 topic pub --rate 10 /cmd_vel_nav geometry_msgs/Twist "{linear: {x: 0.6}, angular: {z: 0.0}}"
```

The `safety_drive_bridge_node` will gate this command through the envelope before forwarding it to `/cmd_vel`.

### Sim only (Gazebo + AGV, no safety nodes)

```bash
ros2 launch safety_core_sim sim_only.launch.py
```

### Safety nodes only (for replay / hardware testing)

```bash
ros2 launch safety_core_bringup safety_only.launch.py
```

## Topics

### Inbound (consumed by the safety stack)

| Topic                | Type                          | Source                  | Notes                                         |
|----------------------|-------------------------------|-------------------------|-----------------------------------------------|
| `/scan`              | `sensor_msgs/LaserScan`       | Gazebo (gpu_lidar)      | 360 samples, 15 Hz, 10 m max range            |
| `/odom`              | `nav_msgs/Odometry`           | Gazebo (DiffDrive)      | 50 Hz wheel odometry                          |
| `/imu`               | `sensor_msgs/Imu`             | Gazebo (Imu sensor)     | 100 Hz                                        |
| `/cmd_vel_nav`       | `geometry_msgs/Twist`         | Nav2 collision_monitor  | Gated by safety_drive_bridge before `/cmd_vel`|

### Outbound (published by safety stack)

| Topic                       | Type                                      | Publisher                  |
|-----------------------------|-------------------------------------------|----------------------------|
| `/safety/envelope_status`   | `safety_core_msgs/EnvelopeStatus`         | `safety_envelope_node`     |
| `/safety/zone_markers`      | `visualization_msgs/MarkerArray`          | `safety_envelope_node`     |
| `/safety/state`             | `safety_core_msgs/SafetyState`            | `safety_supervisor_node`   |
| `/safety/safe_stop`         | `std_msgs/Bool`                           | `safety_supervisor_node`   |
| `/safety/diagnostics`       | `safety_core_msgs/DiagnosticEvent`        | `safety_supervisor_node`   |
| `/cmd_vel`                  | `geometry_msgs/Twist`                     | `safety_drive_bridge_node` |

## Parameters

All wrapper-node parameters live in [`safety_core_bringup/config/safety_params.yaml`](src/safety_core_bringup/config/safety_params.yaml). The defaults match the AGV demo profile in `examples/agv_safety_demo.cpp`:

| Parameter                              | Default | Description                                                |
|----------------------------------------|---------|------------------------------------------------------------|
| `envelope.max_speed_mps`               | 1.5     | Maximum forward speed                                      |
| `envelope.max_comfort_decel_mps2`      | 1.0     | Comfort braking deceleration                               |
| `envelope.safety_buffer_m`             | 0.30    | Minimum clearance to obstacle (defines Emergency boundary) |
| `footprint.length_m` / `width_m`       | 0.80 / 0.60 | Robot bounding box                                     |
| `footprint.front_overhang_m`           | 0.10    | Forward overhang beyond `base_link`                        |
| `corridor_half_width_m`                | 0.40    | Lateral filter for forward-cone scan check                 |
| `localization_timeout_s`               | 0.5     | Threshold for `LocalizationLost` mode                      |
| `cmd_freshness_s`                      | 0.25    | Stale-Nav2-command watchdog                                |
| `max_jerk_mps3`                        | 2.0     | Jerk limit for emergency stop profile                      |

## Verification (without Gazebo)

A quick sanity check that the safety nodes build and launch correctly:

```bash
source install/setup.bash
ros2 launch safety_core_bringup safety_only.launch.py &
sleep 2
ros2 topic list | grep safety
ros2 topic echo /safety/state --once
```

Expected: topics include `/safety/{envelope_status,zone_markers,state,safe_stop,diagnostics}` and the state shows `mode: 1` (Idle), `zone.zone: 0` (Clear), `fault_latched: false`.

## Substituting the AWS RoboMaker warehouse

The included `industrial_warehouse.sdf` is a self-contained SDF that doesn't require external downloads. To use the [AWS RoboMaker small warehouse](https://github.com/aws-robotics/aws-robomaker-small-warehouse-world) instead:

```bash
git clone https://github.com/aws-robotics/aws-robomaker-small-warehouse-world.git \
    ~/aws_warehouse
export GZ_SIM_RESOURCE_PATH=~/aws_warehouse/models:$GZ_SIM_RESOURCE_PATH

ros2 launch safety_core_bringup agv_warehouse.launch.py \
    world:=$HOME/aws_warehouse/worlds/no_roof_small_warehouse/no_roof_small_warehouse.world
```

(The AWS warehouse uses Gazebo Classic SDF; some plugin elements may need updating for Gazebo Harmonic.)

## How safety_core integrates

| safety_core component                              | ROS 2 wrapper                                               |
|----------------------------------------------------|-------------------------------------------------------------|
| `safety_core::platform::Clock`                     | `safety_core_ros::RosClock` (wraps `rclcpp::Clock`)         |
| `safety_core::diag::DiagnosticTransport`           | `safety_core_ros::RosDiagnosticTransport` → ROS topic       |
| `safety_core::diag::HealthMonitor`                 | `safety_core_ros::RosHealthMonitor` → `RCLCPP_*` logs       |
| `safety_core::sm::ModeStateMachine`                | Owned by `safety_supervisor_node`                           |
| `safety_core::safety::SafetySupervisor`            | Owned by `safety_supervisor_node`                           |
| `safety_core::safety::evaluate_stop_distance(...)` | Called per `/scan` in `safety_envelope_node`                |
| `safety_core::motion::generate_jerk_limited_stop_profile()` | Called on `/safety/safe_stop=true` in drive bridge |

The boundary contract: ROS callbacks copy data into safety_core types, run noexcept safety logic, copy results into ROS messages. Diagnostic events are emitted via the pluggable transport so the safety_core core stays decoupled from rclcpp.

## File map

```
ros2/
├── README.md                           ← this file
└── src/
    ├── safety_autonomy_core (symlink)
    ├── safety_core_msgs/
    │   └── msg/{SafetyZone,SafetyState,EnvelopeStatus,DiagnosticEvent,MonitorEvent}.msg
    ├── safety_core_ros/
    │   ├── include/safety_core_ros/
    │   │   ├── ros_clock.hpp
    │   │   ├── ros_diagnostic_transport.hpp
    │   │   ├── ros_health_monitor.hpp
    │   │   ├── safety_envelope_node.hpp
    │   │   ├── safety_supervisor_node.hpp
    │   │   └── safety_drive_bridge_node.hpp
    │   └── src/  (impls + main_*.cpp entry points)
    ├── safety_core_nav2/
    │   ├── include/safety_core_nav2/is_safe_condition.hpp
    │   ├── src/is_safe_condition.cpp
    │   └── safety_core_nav2_bt_plugins.xml
    ├── safety_core_sim/
    │   ├── description/agv.urdf.xacro
    │   ├── worlds/industrial_warehouse.sdf
    │   ├── config/ros_gz_bridge.yaml
    │   └── launch/sim_only.launch.py
    └── safety_core_bringup/
        ├── launch/{agv_warehouse,safety_only}.launch.py
        ├── config/{safety_params,nav2_params,slam_toolbox}.yaml
        └── rviz/agv_warehouse.rviz
```
