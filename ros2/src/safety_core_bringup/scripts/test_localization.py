#!/usr/bin/env python3
"""Localization diagnostics for the safety_core warehouse AGV.

Subscribes to the inputs and outputs of the EKF / navsat_transform pipeline and
prints a periodic dashboard.

* sensor liveness    -- Hz, time since last message, missing-source warnings
* fusion health      -- /odometry/filtered, /odometry/filtered_map, /odometry/gps
* drift estimate     -- Euclidean delta between wheel /odom and /odometry/filtered
* GPS spike monitor  -- residual between /gps and /gps/filtered (>3 sigma flagged)
* TF availability    -- map -> odom and odom -> base_link

It is read-only: it never publishes any control commands and never depends on
Nav2 being up. Run it after the warehouse stack is launched with ``ekf:=true``::

    ros2 launch safety_core_bringup agv_warehouse.launch.py ekf:=true gps:=true
    ros2 run safety_core_bringup test_localization.py     # if installed as exec
    # or, directly:
    python3 ros2/src/safety_core_bringup/scripts/test_localization.py
"""
from __future__ import annotations

import math
import sys
import time
from dataclasses import dataclass, field
from typing import Optional

import rclpy
from geometry_msgs.msg import TransformStamped  # noqa: F401
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import QoSDurabilityPolicy, QoSHistoryPolicy, QoSProfile, QoSReliabilityPolicy
from sensor_msgs.msg import Imu, NavSatFix
from tf2_ros import Buffer, TransformException, TransformListener


SENSOR_QOS = QoSProfile(
    reliability=QoSReliabilityPolicy.BEST_EFFORT,
    history=QoSHistoryPolicy.KEEP_LAST,
    depth=10,
    durability=QoSDurabilityPolicy.VOLATILE,
)


@dataclass
class TopicStats:
    """Tracks Hz and last-message age for a single topic."""

    name: str
    expected_hz: float
    last_stamp: Optional[float] = None
    last_msg_count: int = 0
    msg_count: int = 0
    history: list[float] = field(default_factory=list)

    def record(self, now: float) -> None:
        self.msg_count += 1
        self.last_stamp = now

    def hz(self, window_seconds: float) -> float:
        delta = self.msg_count - self.last_msg_count
        self.last_msg_count = self.msg_count
        return delta / max(window_seconds, 1e-3)

    def age(self, now: float) -> Optional[float]:
        return None if self.last_stamp is None else now - self.last_stamp

    def status(self, now: float, window_seconds: float) -> str:
        age = self.age(now)
        if age is None:
            return f"  [STALE]  {self.name:24s}  no msgs yet  (expected {self.expected_hz:.0f} Hz)"
        hz = self.hz(window_seconds)
        if age > 2.0:
            tag = "[STALE]"
        elif self.expected_hz > 0 and hz < 0.5 * self.expected_hz:
            tag = "[SLOW] "
        else:
            tag = "[OK]   "
        return (
            f"  {tag}  {self.name:24s}  {hz:6.1f} Hz   "
            f"age={age:5.2f}s   total={self.msg_count}"
        )


def _pose_xy(odom: Odometry) -> tuple[float, float]:
    return odom.pose.pose.position.x, odom.pose.pose.position.y


def _yaw(odom: Odometry) -> float:
    q = odom.pose.pose.orientation
    siny_cosp = 2.0 * (q.w * q.z + q.x * q.y)
    cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    return math.atan2(siny_cosp, cosy_cosp)


class LocalizationDiagnostics(Node):
    REPORT_PERIOD_S = 2.0

    def __init__(self) -> None:
        super().__init__("test_localization")
        self.declare_parameter("base_frame", "base_link")
        self.declare_parameter("odom_frame", "odom")
        self.declare_parameter("map_frame", "map")
        self.base_frame = self.get_parameter("base_frame").get_parameter_value().string_value
        self.odom_frame = self.get_parameter("odom_frame").get_parameter_value().string_value
        self.map_frame = self.get_parameter("map_frame").get_parameter_value().string_value

        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)

        self.stats: dict[str, TopicStats] = {
            "/odom": TopicStats("/odom", expected_hz=50.0),
            "/imu": TopicStats("/imu", expected_hz=100.0),
            "/gps": TopicStats("/gps", expected_hz=10.0),
            "/odometry/filtered": TopicStats("/odometry/filtered", expected_hz=50.0),
            "/odometry/filtered_map": TopicStats("/odometry/filtered_map", expected_hz=50.0),
            "/odometry/gps": TopicStats("/odometry/gps", expected_hz=30.0),
        }
        self.latest_odom: Optional[Odometry] = None
        self.latest_filtered: Optional[Odometry] = None
        self.latest_filtered_map: Optional[Odometry] = None
        self.latest_gps_raw: Optional[NavSatFix] = None
        self.latest_gps_filtered: Optional[NavSatFix] = None

        self.gps_residuals: list[float] = []
        self.window_start = self._now()

        self.create_subscription(Odometry, "/odom", self._on_odom, SENSOR_QOS)
        self.create_subscription(Imu, "/imu", self._on_imu, SENSOR_QOS)
        self.create_subscription(NavSatFix, "/gps", self._on_gps_raw, SENSOR_QOS)
        self.create_subscription(NavSatFix, "/gps/filtered", self._on_gps_filtered, SENSOR_QOS)
        self.create_subscription(Odometry, "/odometry/filtered", self._on_filtered, SENSOR_QOS)
        self.create_subscription(
            Odometry, "/odometry/filtered_map", self._on_filtered_map, SENSOR_QOS
        )
        self.create_subscription(Odometry, "/odometry/gps", self._on_odometry_gps, SENSOR_QOS)

        self.create_timer(self.REPORT_PERIOD_S, self._report)
        self.get_logger().info("Localization diagnostics started; reporting every 2 s.")

    # --- callbacks ---------------------------------------------------------
    def _now(self) -> float:
        return time.monotonic()

    def _on_odom(self, msg: Odometry) -> None:
        self.stats["/odom"].record(self._now())
        self.latest_odom = msg

    def _on_imu(self, msg: Imu) -> None:
        self.stats["/imu"].record(self._now())

    def _on_gps_raw(self, msg: NavSatFix) -> None:
        self.stats["/gps"].record(self._now())
        self.latest_gps_raw = msg

    def _on_gps_filtered(self, msg: NavSatFix) -> None:
        self.latest_gps_filtered = msg
        if self.latest_gps_raw is None:
            return
        # Approximate residual (degrees -> meters at ~111 km/deg latitude).
        dlat = (msg.latitude - self.latest_gps_raw.latitude) * 111_320.0
        dlon = (
            (msg.longitude - self.latest_gps_raw.longitude)
            * 111_320.0
            * math.cos(math.radians(msg.latitude))
        )
        self.gps_residuals.append(math.hypot(dlat, dlon))
        if len(self.gps_residuals) > 200:
            self.gps_residuals.pop(0)

    def _on_filtered(self, msg: Odometry) -> None:
        self.stats["/odometry/filtered"].record(self._now())
        self.latest_filtered = msg

    def _on_filtered_map(self, msg: Odometry) -> None:
        self.stats["/odometry/filtered_map"].record(self._now())
        self.latest_filtered_map = msg

    def _on_odometry_gps(self, msg: Odometry) -> None:
        self.stats["/odometry/gps"].record(self._now())

    # --- reporting ---------------------------------------------------------
    def _drift(self) -> Optional[float]:
        if self.latest_odom is None or self.latest_filtered is None:
            return None
        ox, oy = _pose_xy(self.latest_odom)
        fx, fy = _pose_xy(self.latest_filtered)
        return math.hypot(ox - fx, oy - fy)

    def _gps_outlier_summary(self) -> str:
        if not self.gps_residuals:
            return "  /gps spike monitor:    no /gps/filtered samples yet"
        latest = self.gps_residuals[-1]
        avg = sum(self.gps_residuals) / len(self.gps_residuals)
        peak = max(self.gps_residuals)
        n_outliers = sum(1 for r in self.gps_residuals if r > 3.0)  # 3 sigma at stddev=1.0 m
        flag = "  [WARN] " if n_outliers > 0 else "  [OK]   "
        return (
            f"{flag}/gps spike monitor:    "
            f"latest_residual={latest:.2f} m  avg={avg:.2f} m  peak={peak:.2f} m  "
            f">3σ_count={n_outliers}/{len(self.gps_residuals)}"
        )

    def _tf_status(self, src: str, dst: str) -> str:
        try:
            self.tf_buffer.lookup_transform(
                src, dst, rclpy.time.Time(), rclpy.duration.Duration(seconds=0.0)
            )
            return f"  [OK]    TF {src} -> {dst}: present"
        except TransformException as exc:
            return f"  [MISS]  TF {src} -> {dst}: {exc}"

    def _report(self) -> None:
        now = self._now()
        window = now - self.window_start
        self.window_start = now

        lines = ["", "==== safety_core localization diagnostics ===="]
        lines.append("Sensor liveness:")
        for stat in (
            self.stats["/odom"],
            self.stats["/imu"],
            self.stats["/gps"],
        ):
            lines.append(stat.status(now, window))
        lines.append("Fusion outputs:")
        for stat in (
            self.stats["/odometry/filtered"],
            self.stats["/odometry/filtered_map"],
            self.stats["/odometry/gps"],
        ):
            lines.append(stat.status(now, window))
        lines.append("Drift / fusion quality:")
        drift = self._drift()
        if drift is None:
            lines.append("  [WAIT]  /odom vs /odometry/filtered: not enough data yet")
        else:
            tag = "[OK]   " if drift < 1.0 else ("[WARN] " if drift < 5.0 else "[FAIL] ")
            lines.append(
                f"  {tag} /odom vs /odometry/filtered: planar delta = {drift:.3f} m"
            )
        lines.append(self._gps_outlier_summary())
        lines.append("TF chain:")
        lines.append(self._tf_status(self.map_frame, self.odom_frame))
        lines.append(self._tf_status(self.odom_frame, self.base_frame))
        lines.append("=" * 50)
        self.get_logger().info("\n".join(lines))


def main(argv: Optional[list[str]] = None) -> int:
    rclpy.init(args=argv if argv is not None else sys.argv)
    node = LocalizationDiagnostics()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
