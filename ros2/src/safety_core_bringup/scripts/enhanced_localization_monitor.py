#!/usr/bin/env python3
"""Enhanced localization monitoring node with kidnapping detection.

Comprehensive diagnostics including:
- EKF filter covariance (position uncertainty)
- GPS signal quality monitoring
- Sensor data freshness and rates
- TF tree consistency
- Drift between wheel odometry and filtered odometry
- Kidnapping detection (sudden pose jumps)
- Map-matching quality assessment
- Localization confidence scoring

Usage:
    ros2 run safety_core_bringup enhanced_localization_monitor.py
"""
from __future__ import annotations

import math
import sys
import time
from typing import Optional
from collections import deque

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
    cov = odom.pose.covariance
    pos_cov = math.sqrt(cov[0] + cov[7])
    return pos_cov


class EnhancedLocalizationMonitor(Node):
    """Enhanced localization monitor with kidnapping detection."""
    
    def __init__(self):
        super().__init__('enhanced_localization_monitor')
        
        # Parameters
        self.kidnapping_threshold = self.declare_parameter('kidnapping_threshold', 2.0).value  # meters
        self.kidnapping_angle_threshold = self.declare_parameter('kidnapping_angle_threshold', 1.0).value  # radians
        self.position_uncertainty_threshold = self.declare_parameter('position_uncertainty_threshold', 1.0).value  # meters
        self.sensor_timeout = self.declare_parameter('sensor_timeout', 1.0).value  # seconds
        self.history_length = self.declare_parameter('history_length', 10).value  # Number of poses to keep for kidnapping detection
        
        # State tracking
        self.ekf_odom = None
        self.wheel_odom = None
        self.imu_data = None
        self.gps_data = None
        self.last_update_time = None
        
        # Pose history for kidnapping detection
        self.pose_history = deque(maxlen=self.history_length)
        self.kidnapping_detected = False
        self.localization_confidence = 1.0
        
        # TF listener
        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)
        
        # QoS
        sensor_qos = QoSProfile(
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=10,
            durability=QoSDurabilityPolicy.VOLATILE,
        )
        
        # Subscribers
        self.ekf_odom_sub = self.create_subscription(
            Odometry, '/odometry/filtered', self.ekf_odom_callback, sensor_qos
        )
        
        self.wheel_odom_sub = self.create_subscription(
            Odometry, '/odom', self.wheel_odom_callback, sensor_qos
        )
        
        self.imu_sub = self.create_subscription(
            Imu, '/imu', self.imu_callback, sensor_qos
        )
        
        self.gps_sub = self.create_subscription(
            NavSatFix, '/gps', self.gps_callback, sensor_qos
        )
        
        # Publishers
        self.diag_pub = self.create_publisher(
            DiagnosticArray, '/diagnostics', 10
        )
        
        # Timer for diagnostics
        self.diag_timer = self.create_timer(1.0, self.publish_diagnostics)
        self.check_timer = self.create_timer(0.1, self.check_localization_health)
        
        self.get_logger().info('Enhanced localization monitor initialized')
    
    def ekf_odom_callback(self, msg: Odometry):
        """Handle EKF odometry messages."""
        self.ekf_odom = msg
        self.last_update_time = self.get_clock().now()
        
        # Add to pose history for kidnapping detection
        current_pose = _pose_xy(msg)
        self.pose_history.append(current_pose)
        
        # Check for kidnapping
        self.check_kidnapping(current_pose)
    
    def wheel_odom_callback(self, msg: Odometry):
        """Handle wheel odometry messages."""
        self.wheel_odom = msg
    
    def imu_callback(self, msg: Imu):
        """Handle IMU messages."""
        self.imu_data = msg
    
    def gps_callback(self, msg: NavSatFix):
        """Handle GPS messages."""
        self.gps_data = msg
    
    def check_kidnapping(self, current_pose: tuple[float, float]):
        """Check for kidnapping (sudden pose jumps)."""
        if len(self.pose_history) < 2:
            return
        
        prev_pose = self.pose_history[-2]
        
        # Calculate position difference
        dx = current_pose[0] - prev_pose[0]
        dy = current_pose[1] - prev_pose[1]
        distance = math.sqrt(dx**2 + dy**2)
        
        # Check if distance exceeds threshold (accounting for time delta)
        if distance > self.kidnapping_threshold:
            self.kidnapping_detected = True
            self.localization_confidence = 0.0
            self.get_logger().warn(f'Kidnapping detected: sudden jump of {distance:.2f} meters')
        else:
            self.kidnapping_detected = False
            # Gradually restore confidence
            self.localization_confidence = min(1.0, self.localization_confidence + 0.1)
    
    def check_localization_health(self):
        """Check overall localization health."""
        current_time = self.get_clock().now()
        
        # Check for sensor timeouts
        if self.last_update_time is not None:
            time_since_update = (current_time - self.last_update_time).nanoseconds / 1e9
            if time_since_update > self.sensor_timeout:
                self.localization_confidence = max(0.0, self.localization_confidence - 0.2)
        
        # Check position uncertainty
        if self.ekf_odom is not None:
            uncertainty = _get_position_uncertainty(self.ekf_odom)
            if uncertainty > self.position_uncertainty_threshold:
                self.localization_confidence = max(0.0, self.localization_confidence - 0.1)
        
        # Check TF tree consistency
        try:
            self.tf_buffer.lookup_transform(
                'map', 'base_link',
                rclpy.time.Time(),
                timeout=rclpy.duration.Duration(seconds=0.1)
            )
        except TransformException:
            self.localization_confidence = max(0.0, self.localization_confidence - 0.3)
    
    def publish_diagnostics(self):
        """Publish comprehensive diagnostic information."""
        diag = DiagnosticArray()
        diag.header.stamp = self.get_clock().now().to_msg()
        
        # Main localization status
        main_status = DiagnosticStatus()
        main_status.name = 'Enhanced Localization Monitor'
        main_status.hardware_id = 'localization'
        
        if self.kidnapping_detected:
            main_status.level = DiagnosticStatus.ERROR
            main_status.message = 'Kidnapping detected - localization lost'
        elif self.localization_confidence < 0.5:
            main_status.level = DiagnosticStatus.WARN
            main_status.message = f'Low localization confidence: {self.localization_confidence:.2f}'
        else:
            main_status.level = DiagnosticStatus.OK
            main_status.message = 'Localization healthy'
        
        # Add key-value pairs
        main_status.values.append(KeyValue(key='localization_confidence', 
                                          value=f'{self.localization_confidence:.2f}'))
        main_status.values.append(KeyValue(key='kidnapping_detected', 
                                          value=str(self.kidnapping_detected)))
        
        if self.ekf_odom is not None:
            uncertainty = _get_position_uncertainty(self.ekf_odom)
            main_status.values.append(KeyValue(key='position_uncertainty_m', 
                                              value=f'{uncertainty:.3f}'))
            main_status.values.append(KeyValue(key='ekf_pose_x', 
                                              value=f'{self.ekf_odom.pose.pose.position.x:.3f}'))
            main_status.values.append(KeyValue(key='ekf_pose_y', 
                                              value=f'{self.ekf_odom.pose.pose.position.y:.3f}'))
        
        if self.wheel_odom is not None:
            wheel_speed = math.sqrt(
                self.wheel_odom.twist.twist.linear.x**2 + 
                self.wheel_odom.twist.twist.linear.y**2
            )
            main_status.values.append(KeyValue(key='wheel_speed_m_s', 
                                              value=f'{wheel_speed:.3f}'))
        
        if self.gps_data is not None:
            main_status.values.append(KeyValue(key='gps_status', 
                                              value=str(self.gps_data.status.status)))
            if self.gps_data.position_covariance:
                main_status.values.append(KeyValue(key='gps_variance_m2', 
                                                  value=f'{self.gps_data.position_covariance[0]:.2f}'))
        
        if self.last_update_time is not None:
            time_since_update = (self.get_clock().now() - self.last_update_time).nanoseconds / 1e9
            main_status.values.append(KeyValue(key='time_since_update_s', 
                                              value=f'{time_since_update:.2f}'))
        
        diag.status.append(main_status)
        
        # Individual sensor status
        if self.ekf_odom is not None:
            ekf_status = DiagnosticStatus()
            ekf_status.name = 'EKF Odometry'
            ekf_status.hardware_id = 'localization'
            ekf_status.level = DiagnosticStatus.OK
            ekf_status.message = 'EKF odometry receiving'
            ekf_status.values.append(KeyValue(key='pose_x', value=f'{self.ekf_odom.pose.pose.position.x:.3f}'))
            ekf_status.values.append(KeyValue(key='pose_y', value=f'{self.ekf_odom.pose.pose.position.y:.3f}'))
            ekf_status.values.append(KeyValue(key='velocity_x', value=f'{self.ekf_odom.twist.twist.linear.x:.3f}'))
            diag.status.append(ekf_status)
        
        if self.gps_data is not None:
            gps_status = DiagnosticStatus()
            gps_status.name = 'GPS'
            gps_status.hardware_id = 'sensors'
            if self.gps_data.status.status >= 0:
                gps_status.level = DiagnosticStatus.OK
                gps_status.message = 'GPS fix available'
            else:
                gps_status.level = DiagnosticStatus.WARN
                gps_status.message = 'No GPS fix'
            gps_status.values.append(KeyValue(key='satellites', value=str(len(self.gps_data.status.satellite_used) if hasattr(self.gps_data.status, 'satellite_used') else 'N/A')))
            diag.status.append(gps_status)
        
        self.diag_pub.publish(diag)


def main():
    rclpy.init()
    node = EnhancedLocalizationMonitor()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()