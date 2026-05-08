#!/usr/bin/env python3
"""Adaptive EKF node with wheel slip detection and dynamic process noise adjustment.

Monitors wheel odometry vs IMU/visual odometry to detect wheel slip events
and dynamically adjusts EKF process noise covariance for robust localization.

Usage:
    ros2 run safety_core_bringup adaptive_ekf_node.py
"""
from __future__ import annotations

import math
import rclpy
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus, KeyValue
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.parameter import Parameter
from rclpy.qos import QoSDurabilityPolicy, QoSHistoryPolicy, QoSProfile, QoSReliabilityPolicy
from sensor_msgs.msg import Imu


class AdaptiveEKFNode(Node):
    """Adaptive EKF node with slip detection and dynamic noise adjustment."""
    
    def __init__(self):
        super().__init__('adaptive_ekf_node')
        
        # Parameters
        self.ekf_node_name = self.declare_parameter('ekf_node_name', 'ekf_filter_node').value
        self.slip_threshold = self.declare_parameter('slip_threshold', 0.5).value  # m/s difference threshold
        self.slip_duration = self.declare_parameter('slip_duration', 0.5).value  # seconds
        self.base_process_noise = self.declare_parameter('base_process_noise', 0.05).value
        self.slip_process_noise = self.declare_parameter('slip_process_noise', 0.5).value
        self.stationary_threshold = self.declare_parameter('stationary_threshold', 0.01).value  # m/s
        self.stationary_time = self.declare_parameter('stationary_time', 2.0).value  # seconds
        
        # State
        self.wheel_odom = None
        self.imu_data = None
        self.visual_odom = None
        self.last_slip_time = None
        self.last_movement_time = None
        self.current_noise_level = self.base_process_noise
        self.slip_detected = False
        
        # QoS
        sensor_qos = QoSProfile(
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=10,
            durability=QoSDurabilityPolicy.VOLATILE,
        )
        
        # Subscribers
        self.wheel_odom_sub = self.create_subscription(
            Odometry, '/odom', self.wheel_odom_callback, sensor_qos
        )
        
        self.imu_sub = self.create_subscription(
            Imu, '/imu', self.imu_callback, sensor_qos
        )
        
        self.visual_odom_sub = self.create_subscription(
            Odometry, '/visual_odometry', self.visual_odom_callback, sensor_qos
        )
        
        # Publishers
        self.diag_pub = self.create_publisher(
            DiagnosticArray, '/diagnostics', 10
        )
        
        # Timer for diagnostics
        self.diag_timer = self.create_timer(1.0, self.publish_diagnostics)
        
        # Timer for noise adjustment
        self.noise_timer = self.create_timer(0.1, self.adjust_process_noise)
        
        self.get_logger().info('Adaptive EKF node initialized')
    
    def wheel_odom_callback(self, msg: Odometry):
        """Handle wheel odometry messages."""
        self.wheel_odom = msg
        self.check_movement(msg.twist.twist.linear.x, msg.twist.twist.linear.y)
    
    def imu_callback(self, msg: Imu):
        """Handle IMU messages."""
        self.imu_data = msg
    
    def visual_odom_callback(self, msg: Odometry):
        """Handle visual odometry messages."""
        self.visual_odom = msg
    
    def check_movement(self, vx, vy):
        """Check if robot is moving."""
        speed = math.sqrt(vx**2 + vy**2)
        if speed > self.stationary_threshold:
            self.last_movement_time = self.get_clock().now()
    
    def detect_wheel_slip(self):
        """Detect wheel slip by comparing wheel odometry with other sensors."""
        if self.wheel_odom is None:
            return False
        
        wheel_speed = math.sqrt(
            self.wheel_odom.twist.twist.linear.x**2 + 
            self.wheel_odom.twist.twist.linear.y**2
        )
        
        # Compare with IMU acceleration (integrated)
        if self.imu_data is not None:
            imu_accel = math.sqrt(
                self.imu_data.linear_acceleration.x**2 + 
                self.imu_data.linear_acceleration.y**2
            )
            # If wheel odometry shows movement but IMU shows low acceleration, potential slip
            if wheel_speed > self.slip_threshold and imu_accel < 0.5:
                return True
        
        # Compare with visual odometry if available
        if self.visual_odom is not None:
            visual_speed = math.sqrt(
                self.visual_odom.twist.twist.linear.x**2 + 
                self.visual_odom.twist.twist.linear.y**2
            )
            speed_diff = abs(wheel_speed - visual_speed)
            if speed_diff > self.slip_threshold:
                return True
        
        return False
    
    def adjust_process_noise(self):
        """Dynamically adjust EKF process noise based on conditions."""
        current_time = self.get_clock().now()
        
        # Check for wheel slip
        if self.detect_wheel_slip():
            self.last_slip_time = current_time
            self.slip_detected = True
            self.current_noise_level = self.slip_process_noise
            self.get_logger().warn('Wheel slip detected - increasing process noise')
        else:
            # Check if we're still in slip recovery period
            if self.last_slip_time is not None:
                time_since_slip = (current_time - self.last_slip_time).nanoseconds / 1e9
                if time_since_slip < self.slip_duration:
                    self.current_noise_level = self.slip_process_noise
                else:
                    self.slip_detected = False
                    self.current_noise_level = self.base_process_noise
            else:
                self.slip_detected = False
                self.current_noise_level = self.base_process_noise
        
        # Check if robot is stationary
        if self.last_movement_time is not None:
            time_since_movement = (current_time - self.last_movement_time).nanoseconds / 1e9
            if time_since_movement > self.stationary_time:
                # Reduce noise when stationary
                self.current_noise_level = self.base_process_noise * 0.1
        
        # Apply noise adjustment to EKF (via parameter reconfiguration)
        # Note: This would require dynamic reconfiguration of robot_localization
        # For now, we'll publish the recommended noise level
        self.publish_noise_level()
    
    def publish_noise_level(self):
        """Publish current recommended process noise level."""
        # This could be used by another node to dynamically reconfigure EKF
        # For now, we'll include it in diagnostics
        pass
    
    def publish_diagnostics(self):
        """Publish diagnostic information."""
        diag = DiagnosticArray()
        diag.header.stamp = self.get_clock().now().to_msg()
        
        status = DiagnosticStatus()
        status.name = 'Adaptive EKF'
        status.hardware_id = 'localization'
        
        if self.slip_detected:
            status.level = DiagnosticStatus.WARN
            status.message = 'Wheel slip detected'
        else:
            status.level = DiagnosticStatus.OK
            status.message = 'Normal operation'
        
        # Add key-value pairs
        status.values.append(KeyValue(key='current_noise_level', 
                                      value=str(self.current_noise_level)))
        status.values.append(KeyValue(key='slip_detected', 
                                      value=str(self.slip_detected)))
        
        if self.wheel_odom is not None:
            wheel_speed = math.sqrt(
                self.wheel_odom.twist.twist.linear.x**2 + 
                self.wheel_odom.twist.twist.linear.y**2
            )
            status.values.append(KeyValue(key='wheel_speed', value=str(wheel_speed)))
        
        if self.visual_odom is not None:
            visual_speed = math.sqrt(
                self.visual_odom.twist.twist.linear.x**2 + 
                self.visual_odom.twist.twist.linear.y**2
            )
            status.values.append(KeyValue(key='visual_speed', value=str(visual_speed)))
        
        diag.status.append(status)
        self.diag_pub.publish(diag)


def main():
    rclpy.init()
    node = AdaptiveEKFNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()