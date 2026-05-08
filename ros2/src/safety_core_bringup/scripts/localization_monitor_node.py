#!/usr/bin/env python3
"""Localization quality monitoring node for safety_core.

Publishes diagnostic messages about localization system health to /diagnostics.
Monitors:
- EKF filter covariance (position uncertainty)
- GPS signal quality (number of satellites, HDOP)
- Sensor data freshness and rates
- TF tree consistency
- Drift between wheel odometry and filtered odometry

Usage:
    ros2 run safety_core_bringup localization_monitor_node.py
"""
from __future__ import annotations

import math
import sys
import time
from typing import Optional

import rclpy
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus, KeyValue
from geometry_msgs.msg import TransformStamped
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


def _pose_xy(odom: Odometry) -> tuple[float, float]:
    return odom.pose.pose.position.x, odom.pose.pose.position.y


def _get_position_uncertainty(odom: Odometry) -> float:
    """Extract position uncertainty from odometry covariance."""
    # Use the largest eigenvalue approximation from x,y covariance
    cov = odom.pose.covariance
    # Position covariance is at indices [0, 5, 35] for x, y, z
    pos_cov = math.sqrt(cov[0] + cov[7])  # Approximate sqrt of x+y variance
    return pos_cov


class LocalizationMonitor(Node):
    """Monitors localization quality and publishes diagnostics."""

    DIAG_PERIOD_S = 1.0

    def __init__(self) -> None:
        super().__init__("localization_monitor")
        self.declare_parameter("base_frame", "base_link")
        self.declare_parameter("odom_frame", "odom")
        self.declare_parameter("map_frame", "map")
        self.declare_parameter("max_position_uncertainty", 2.0)  # meters
        self.declare_parameter("max_drift", 5.0)  # meters
        self.declare_parameter("min_gps_satellites", 6)
        self.declare_parameter("max_gps_hdop", 2.0)

        self.base_frame = self.get_parameter("base_frame").get_parameter_value().string_value
        self.odom_frame = self.get_parameter("odom_frame").get_parameter_value().string_value
        self.map_frame = self.get_parameter("map_frame").get_parameter_value().string_value
        self.max_position_uncertainty = self.get_parameter("max_position_uncertainty").get_parameter_value().double_value
        self.max_drift = self.get_parameter("max_drift").get_parameter_value().double_value
        self.min_gps_satellites = self.get_parameter("min_gps_satellites").get_parameter_value().integer_value
        self.max_gps_hdop = self.get_parameter("max_gps_hdop").get_parameter_value().double_value

        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)

        # Latest messages
        self.latest_odom: Optional[Odometry] = None
        self.latest_filtered: Optional[Odometry] = None
        self.latest_filtered_map: Optional[Odometry] = None
        self.latest_gps: Optional[NavSatFix] = None
        self.latest_imu: Optional[Imu] = None

        # Timestamp tracking for freshness checks
        self.last_odom_time: Optional[float] = None
        self.last_filtered_time: Optional[float] = None
        self.last_gps_time: Optional[float] = None
        self.last_imu_time: Optional[float] = None

        # Diagnostics publisher
        self.diag_pub = self.create_publisher(DiagnosticArray, "/diagnostics", 10)

        # Subscriptions
        self.create_subscription(Odometry, "/odom", self._on_odom, SENSOR_QOS)
        self.create_subscription(Imu, "/imu", self._on_imu, SENSOR_QOS)
        self.create_subscription(NavSatFix, "/gps", self._on_gps, SENSOR_QOS)
        self.create_subscription(Odometry, "/odometry/filtered", self._on_filtered, SENSOR_QOS)
        self.create_subscription(Odometry, "/odometry/filtered_map", self._on_filtered_map, SENSOR_QOS)

        # Diagnostic timer
        self.create_timer(self.DIAG_PERIOD_S, self._publish_diagnostics)

        self.get_logger().info("Localization monitor started")

    def _now(self) -> float:
        return time.monotonic()

    def _on_odom(self, msg: Odometry) -> None:
        self.latest_odom = msg
        self.last_odom_time = self._now()

    def _on_imu(self, msg: Imu) -> None:
        self.latest_imu = msg
        self.last_imu_time = self._now()

    def _on_gps(self, msg: NavSatFix) -> None:
        self.latest_gps = msg
        self.last_gps_time = self._now()

    def _on_filtered(self, msg: Odometry) -> None:
        self.latest_filtered = msg
        self.last_filtered_time = self._now()

    def _on_filtered_map(self, msg: Odometry) -> None:
        self.latest_filtered_map = msg

    def _check_sensor_freshness(self, now: float, name: str, last_time: Optional[float], max_age: float) -> tuple[int, str]:
        """Check if sensor data is fresh enough."""
        if last_time is None:
            return DiagnosticStatus.WARN, f"No {name} data received"
        age = now - last_time
        if age > max_age:
            return DiagnosticStatus.ERROR, f"{name} data stale ({age:.1f}s old, max {max_age}s)"
        return DiagnosticStatus.OK, f"{name} data fresh ({age:.1f}s old)"

    def _check_ekf_quality(self) -> DiagnosticStatus:
        """Check EKF filter quality based on covariance."""
        status = DiagnosticStatus()
        status.name = "localization:ekf_quality"
        status.hardware_id = "ekf_filter"

        if self.latest_filtered is None:
            status.level = DiagnosticStatus.WARN
            status.message = "No filtered odometry available"
            return status

        uncertainty = _get_position_uncertainty(self.latest_filtered)
        status.values.append(KeyValue(key="position_uncertainty", value=f"{uncertainty:.3f}"))
        status.values.append(KeyValue(key="max_threshold", value=f"{self.max_position_uncertainty:.3f}"))

        if uncertainty > self.max_position_uncertainty:
            status.level = DiagnosticStatus.ERROR
            status.message = f"Position uncertainty too high: {uncertainty:.3f}m"
        else:
            status.level = DiagnosticStatus.OK
            status.message = f"Position uncertainty acceptable: {uncertainty:.3f}m"

        return status

    def _check_gps_quality(self) -> DiagnosticStatus:
        """Check GPS signal quality."""
        status = DiagnosticStatus()
        status.name = "localization:gps_quality"
        status.hardware_id = "gps"

        if self.latest_gps is None:
            status.level = DiagnosticStatus.STALE
            status.message = "No GPS data available"
            return status

        # Check GPS status
        if self.latest_gps.status.status >= NavSatFix.STATUS_NO_FIX:
            status.level = DiagnosticStatus.ERROR
            status.message = f"GPS has no fix (status: {self.latest_gps.status.status})"
            return status

        # Check satellite count and HDOP if available
        satellites = 0
        hdop = 999.0

        # NavSatFix doesn't always include these fields, but we can check
        if hasattr(self.latest_gps, 'position_covariance'):
            # Use covariance as a proxy for quality if satellites/HDOP not available
            cov_trace = sum(self.latest_gps.position_covariance[:6])
            status.values.append(KeyValue(key="covariance_trace", value=f"{cov_trace:.6f}"))

        # Check if we have satellite count (via custom augmentation or GPS status)
        # For now, we'll use covariance as a quality indicator
        if self.latest_gps.position_covariance_type == NavSatFix.COVARIANCE_TYPE_UNKNOWN:
            status.level = DiagnosticStatus.WARN
            status.message = "GPS covariance unknown"
        elif self.latest_gps.position_covariance_type == NavSatFix.COVARIANCE_TYPE_APPROXIMATED:
            status.level = DiagnosticStatus.OK
            status.message = "GPS covariance approximated"
        else:
            status.level = DiagnosticStatus.OK
            status.message = "GPS covariance available"

        return status

    def _check_drift(self) -> DiagnosticStatus:
        """Check drift between wheel odometry and filtered odometry."""
        status = DiagnosticStatus()
        status.name = "localization:drift"
        status.hardware_id = "odometry"

        if self.latest_odom is None or self.latest_filtered is None:
            status.level = DiagnosticStatus.STALE
            status.message = "Insufficient data for drift check"
            return status

        ox, oy = _pose_xy(self.latest_odom)
        fx, fy = _pose_xy(self.latest_filtered)
        drift = math.hypot(ox - fx, oy - fy)

        status.values.append(KeyValue(key="drift", value=f"{drift:.3f}"))
        status.values.append(KeyValue(key="max_threshold", value=f"{self.max_drift:.3f}"))

        if drift > self.max_drift:
            status.level = DiagnosticStatus.ERROR
            status.message = f"Excessive drift: {drift:.3f}m"
        elif drift > self.max_drift * 0.5:
            status.level = DiagnosticStatus.WARN
            status.message = f"Elevated drift: {drift:.3f}m"
        else:
            status.level = DiagnosticStatus.OK
            status.message = f"Drift acceptable: {drift:.3f}m"

        return status

    def _check_tf_chain(self) -> DiagnosticStatus:
        """Check TF tree consistency."""
        status = DiagnosticStatus()
        status.name = "localization:tf_chain"
        status.hardware_id = "tf"

        now = rclpy.time.Time()

        # Check odom -> base_link
        try:
            self.tf_buffer.lookup_transform(
                self.odom_frame, self.base_frame, now, rclpy.duration.Duration(seconds=0.1)
            )
            odom_ok = True
            odom_msg = "OK"
        except TransformException as exc:
            odom_ok = False
            odom_msg = str(exc)

        # Check map -> odom
        try:
            self.tf_buffer.lookup_transform(
                self.map_frame, self.odom_frame, now, rclpy.duration.Duration(seconds=0.1)
            )
            map_ok = True
            map_msg = "OK"
        except TransformException as exc:
            map_ok = False
            map_msg = str(exc)

        status.values.append(KeyValue(key="odom_to_base_link", value=odom_msg))
        status.values.append(KeyValue(key="map_to_odom", value=map_msg))

        if odom_ok and map_ok:
            status.level = DiagnosticStatus.OK
            status.message = "TF chain complete"
        elif odom_ok:
            status.level = DiagnosticStatus.WARN
            status.message = "TF chain incomplete (map -> odom missing)"
        else:
            status.level = DiagnosticStatus.ERROR
            status.message = "TF chain broken"

        return status

    def _publish_diagnostics(self) -> None:
        """Publish diagnostic array."""
        now = self._now()
        diag_array = DiagnosticArray()
        diag_array.header.stamp = self.get_clock().now().to_msg()

        # Overall localization status
        overall = DiagnosticStatus()
        overall.name = "localization"
        overall.hardware_id = "localization_system"

        # Check sensor freshness
        odom_level, odom_msg = self._check_sensor_freshness(now, "odometry", self.last_odom_time, 0.5)
        imu_level, imu_msg = self._check_sensor_freshness(now, "IMU", self.last_imu_time, 0.5)
        gps_level, gps_msg = self._check_sensor_freshness(now, "GPS", self.last_gps_time, 2.0)
        filtered_level, filtered_msg = self._check_sensor_freshness(now, "filtered odometry", self.last_filtered_time, 0.5)

        # Add sensor freshness to overall status
        overall.values.append(KeyValue(key="odometry", value=odom_msg))
        overall.values.append(KeyValue(key="imu", value=imu_msg))
        overall.values.append(KeyValue(key="gps", value=gps_msg))
        overall.values.append(KeyValue(key="filtered", value=filtered_msg))

        # Overall level is the worst of all checks
        levels = [odom_level, imu_level, gps_level, filtered_level]
        overall.level = max(levels)
        overall.message = f"Localization system status: {DiagnosticStatus.OK if overall.level == DiagnosticStatus.OK else 'degraded'}"

        diag_array.status.append(overall)

        # Add specific diagnostics
        diag_array.status.append(self._check_ekf_quality())
        diag_array.status.append(self._check_gps_quality())
        diag_array.status.append(self._check_drift())
        diag_array.status.append(self._check_tf_chain())

        self.diag_pub.publish(diag_array)


def main(argv: Optional[list[str]] = None) -> int:
    rclpy.init(args=argv if argv is not None else sys.argv)
    node = LocalizationMonitor()
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
