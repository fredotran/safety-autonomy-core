# Multi-Sensor Fusion Localization

The safety_core warehouse AGV ships an optional [`robot_localization`](https://github.com/cra-ros-pkg/robot_localization)-based localization stack on top of the default wheel-odometry mode.

It fuses **wheel odometry**, **IMU**, and **GPS** into a single drift-corrected pose using two Extended Kalman Filters and a `navsat_transform_node`. The two-EKF + `navsat_transform_node` setup is the pattern recommended by the `robot_localization` author for outdoor / mixed indoor-outdoor robots; it gives the global EKF explicit Mahalanobis-gating control over GPS outliers.

## Architecture

```text
┌─────────────────────── Gazebo Harmonic ──────────────────────────────┐
│ AGV (diff drive, 2D LiDAR, IMU, GPS)                                 │
└──────────────────────────────────────────────────────────────────────┘
        │  /scan, /odom (50 Hz), /imu (100 Hz), /gps (10 Hz)
        ▼
┌────────────────────── ros_gz_bridge ─────────────────────────────────┐
└──────────────────────────────────────────────────────────────────────┘
        │
        ▼
┌──────────────────── ekf_filter_node (odom frame) ────────────────────┐
│  fuses /odom + /imu  ──►  /odometry/filtered                          │
│                       ──►  TF: odom → base_link                       │
└──────────────────────────────────────────────────────────────────────┘
        │
        ├──────────────────────────────────────────┐
        ▼                                          ▼
┌──────────────────── navsat_transform_node ───────┐  /odometry/filtered
│  /imu + /gps + /odometry/filtered                │
│   ──► /odometry/gps    (Odometry in map frame)   │
│   ──► /gps/filtered    (NavSatFix back-projection)
└──────────────────────────────────────────────────┘
        │  /odometry/gps
        ▼
┌──────────────────── ekf_filter_node_map (map frame) ─────────────────┐
│  fuses /odom + /imu + /odometry/gps  ──►  /odometry/filtered_map      │
│                                       ──►  TF: map → odom             │
└──────────────────────────────────────────────────────────────────────┘
```

* The local EKF (`ekf_filter_node`) is the canonical source of `odom → base_link`. Nav2 / collision_monitor / safety_envelope all consume `/odometry/filtered`.
* The global EKF (`ekf_filter_node_map`) is the canonical source of `map → odom`. It only runs when both `ekf:=true` and `gps:=true` (the default when `ekf:=true`).
* GPS outliers are rejected at the global EKF using a Mahalanobis distance threshold of `5.991` (≈ χ²(0.05, dof=2) for the [x, y] subspace).

## Quick start

```bash
# Install the optional dependency:
sudo apt install ros-jazzy-robot-localization

# Re-build the workspace (so the new ekf_config.yaml is installed):
cd ros2
colcon build --packages-select safety_core_bringup
source install/setup.bash

# 1) Full warehouse demo with EKF + GPS fusion:
ros2 launch safety_core_bringup agv_warehouse.launch.py ekf:=true gps:=true

# 2) Same, but skip the GPS branch (single local EKF only):
ros2 launch safety_core_bringup agv_warehouse.launch.py ekf:=true gps:=false

# 3) Stand-alone localization (no Gazebo, no Nav2) for bench testing or bag replay:
ros2 launch safety_core_bringup localization.launch.py gps:=true
```

## Diagnostics

`scripts/test_localization.py` is a read-only diagnostic node that reports a periodic dashboard:

* sensor liveness for `/odom`, `/imu`, `/gps`,
* fusion outputs for `/odometry/filtered`, `/odometry/filtered_map`, `/odometry/gps`,
* drift between wheel `/odom` and the filtered estimate,
* GPS spike monitor (residual between `/gps` and `/gps/filtered`), and
* TF availability for `map → odom` and `odom → base_link`.

```bash
python3 ros2/src/safety_core_bringup/scripts/test_localization.py
```

Sample output:

```
==== safety_core localization diagnostics ====
Sensor liveness:
  [OK]    /odom                      49.8 Hz   age= 0.02s   total=498
  [OK]    /imu                       99.5 Hz   age= 0.01s   total=995
  [OK]    /gps                        9.9 Hz   age= 0.10s   total=99
Fusion outputs:
  [OK]    /odometry/filtered         49.6 Hz   age= 0.02s   total=496
  [OK]    /odometry/filtered_map     49.5 Hz   age= 0.02s   total=495
  [OK]    /odometry/gps              29.7 Hz   age= 0.03s   total=297
Drift / fusion quality:
  [OK]    /odom vs /odometry/filtered: planar delta = 0.043 m
  [OK]    /gps spike monitor:    latest_residual=0.21 m  avg=0.34 m  peak=0.97 m  >3σ_count=0/200
TF chain:
  [OK]    TF map -> odom: present
  [OK]    TF odom -> base_link: present
==================================================
```

## Configuration

All EKF / `navsat_transform` parameters live in `safety_core_bringup/config/ekf_config.yaml`. Highlights:

| Parameter                                  | Value                | Rationale |
|--------------------------------------------|----------------------|-----------|
| `frequency`                                | `50.0`               | matches wheel-odom rate; lower would alias high-frequency IMU |
| `sensor_timeout`                           | `0.2`                | 10 wheel-odom periods; balances dropout-tolerance and stale-state reuse |
| `two_d_mode`                               | `true`               | planar AGV; eliminates ill-observed z / roll / pitch states |
| `odom0_config` (linear-x, yaw-rate only)   | velocities only      | wheel pose drifts; trust velocities, let IMU + GPS correct pose |
| `imu0_config` (yaw, yaw-rate, ax)          | yaw + body rates     | `imu0_relative=true` skips magnetic-north calibration in sim |
| `imu0_remove_gravitational_acceleration`   | `true`               | Gazebo IMU includes gravity; required for correct ax fusion |
| `odom1_pose_rejection_threshold`           | `5.991`              | χ²(0.05, dof=2); rejects GPS spikes >~3σ at stddev=1.0 m |
| `process_noise_covariance` diag            | tuned per-state      | trust velocity / yaw, allow position drift to be absorbed by sensors |

### Tuning workflow

1. **Run the warehouse demo** with `ekf:=true gps:=true` and let the AGV drive a closed loop.
2. **Watch `scripts/test_localization.py`**: drift > 1 m or GPS-residual avg > 1 m indicates retuning is needed.
3. **Increase `process_noise_covariance` diagonals** (e.g. `0.06 → 0.10`) if filter is too stiff (oscillates around sensor measurements). **Decrease** them if filter is too loose (lags behind ground truth).
4. **Tighten `odom1_pose_rejection_threshold`** (`5.991 → 3.84` ≈ χ²(0.10, dof=2)) if GPS outliers still leak through; **loosen** it (`5.991 → 9.21` ≈ χ²(0.01, dof=2)) if good fixes are being rejected during fast turns.
5. **For physical robots**, override `magnetic_declination_radians` and `yaw_offset` for `navsat_transform` to align IMU heading with map north.

## Failure modes & expected behaviour

| Scenario                          | Local EKF (`/odometry/filtered`) | Global EKF (`/odometry/filtered_map`) | TF                          |
|-----------------------------------|-------------------------------------|---------------------------------------------|-----------------------------|
| All sensors healthy               | tracks ground truth                 | tracks ground truth                         | `map → odom → base_link` ok |
| GPS dropout (`/gps` stops)        | unaffected                          | gracefully reverts to dead-reckoning        | `map → odom` may freeze (last-known) |
| GPS spike (huge jump in NavSatFix)| unaffected                          | rejected by Mahalanobis gate; no jump        | unchanged                   |
| IMU dropout (`/imu` stops)        | yaw drifts (wheel-odom only)        | yaw drifts                                   | unchanged                   |
| Wheel-odom dropout (`/odom` stops)| degrades after `sensor_timeout`     | degrades after `sensor_timeout`              | unchanged until timeout     |
| `/clock` jump back (sim restart)  | filter resets (`reset_on_time_jump=true`) | filter resets                          | re-broadcast on next update |

## Troubleshooting

* **`Could not find a package configuration file provided by "robot_localization"`** — install the apt package: `sudo apt install ros-jazzy-robot-localization`.
* **No `map → odom` TF** — confirm `gps:=true` and that `/gps` is actually publishing fixes. `navsat_transform_node` waits `delay=3.0 s` for its first fix before starting to publish.
* **Filter diverges immediately** — usually a covariance mismatch on `/odom` or `/imu`. Print the messages with `ros2 topic echo` and check that twist covariances are non-zero (Gazebo plugins sometimes report all zeros).
* **`/odometry/filtered_map` jumps when entering / leaving GPS-shadowed zones** — increase `odom1_pose_rejection_threshold` (loosen) or run only the local EKF (`gps:=false`) for purely indoor sections.
