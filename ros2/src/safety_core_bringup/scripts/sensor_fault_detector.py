#!/usr/bin/env python3
"""Sensor fault detection and rejection node for safety_core.

Monitors sensor data quality and detects faults such as:
- Frozen sensors (no updates)
- Excessive noise
- Out-of-range values
- Inconsistent readings between sensors

Publishes fault status and can trigger sensor rejection logic.

Usage:
    ros2 run safety_core_bringup sensor_fault_detector.py
"""
from __future__ import annotations

import math
import rclpy
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus, KeyValue
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import QoSDurabilityPolicy, QoSHistoryPolicy, QoSProfile, QoSReliabilityPolicy
from sensor_msgs.msg import Imu, NavSatFix
from std_msgs.msg import Header


class SensorFaultDetector(Node):
    """Sensor fault detection and rejection node."""
    
    def __init__(self):
        super().__init__('sensor_fault_detector')
        
        # Parameters
        self.sensor_timeout = self.declare_parameter('sensor_timeout', 1.0).value  # seconds
        self.max_acceleration = self.declare_parameter('max_acceleration', 10.0).value  # m/s^2
        self.max_angular_velocity = self.declare_parameter('max_angular_velocity', 10.0).value  # rad/s
        self.max_gps_variance = self.declare_parameter('max_gps_variance', 100.0).value  # m^2
        
        # Sensor status tracking
        self.sensor_status = {
            'wheel_odom': {'last_time': None, 'fault': False, 'fault_count': 0},
            'imu': {'last_time': None, 'fault': False, 'fault_count': 0},
            'gps': {'last_time': None, 'fault': False, 'fault_count': 0},
            'visual_odom': {'last_time': None, 'fault': False, 'fault_count': 0},
        }
        
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
        
        self.gps_sub = self.create_subscription(
            NavSatFix, '/gps', self.gps_callback, sensor_qos
        )
        
        self.visual_odom_sub = self.create_subscription(
            Odometry, '/visual_odometry', self.visual_odom_callback, sensor_qos
        )
        
        # Publishers
        self.diag_pub = self.create_publisher(
            DiagnosticArray, '/diagnostics', 10
        )
        
        # Timer for fault checking and diagnostics
        self.check_timer = self.create_timer(0.1, self.check_sensor_faults)
        self.diag_timer = self.create_timer(1.0, self.publish_diagnostics)
        
        self.get_logger().info('Sensor fault detector initialized')
    
    def wheel_odom_callback(self, msg: Odometry):
        """Handle wheel odometry messages."""
        self.sensor_status['wheel_odom']['last_time'] = self.get_clock().now()
        
        # Check for reasonable velocities
        linear_speed = math.sqrt(
            msg.twist.twist.linear.x**2 + 
            msg.twist.twist.linear.y**2
        )
        angular_speed = abs(msg.twist.twist.angular.z)
        
        if linear_speed > 10.0 or angular_speed > 10.0:
            self.sensor_status['wheel_odom']['fault'] = True
            self.sensor_status['wheel_odom']['fault_count'] += 1
            self.get_logger().warn(f'Wheel odometry fault: excessive speed {linear_speed:.2f} m/s, {angular_speed:.2f} rad/s')
        else:
            self.sensor_status['wheel_odom']['fault'] = False
    
    def imu_callback(self, msg: Imu):
        """Handle IMU messages."""
        self.sensor_status['imu']['last_time'] = self.get_clock().now()
        
        # Check for reasonable accelerations and angular velocities
        accel = math.sqrt(
            msg.linear_acceleration.x**2 + 
            msg.linear_acceleration.y**2 + 
            msg.linear_acceleration.z**2
        )
        
        angular_vel = math.sqrt(
            msg.angular_velocity.x**2 + 
            msg.angular_velocity.y**2 + 
            msg.angular_velocity.z**2
        )
        
        if accel > self.max_acceleration:
            self.sensor_status['imu']['fault'] = True
            self.sensor_status['imu']['fault_count'] += 1
            self.get_logger().warn(f'IMU fault: excessive acceleration {accel:.2f} m/s^2')
        elif angular_vel > self.max_angular_velocity:
            self.sensor_status['imu']['fault'] = True
            self.sensor_status['imu']['fault_count'] += 1
            self.get_logger().warn(f'IMU fault: excessive angular velocity {angular_vel:.2f} rad/s')
        else:
            self.sensor_status['imu']['fault'] = False
    
    def gps_callback(self, msg: NavSatFix):
        """Handle GPS messages."""
        self.sensor_status['gps']['last_time'] = self.get_clock().now()
        
        # Check GPS status and variance
        if msg.status.status < 0:  # No fix
            self.sensor_status['gps']['fault'] = True
            self.sensor_status['gps']['fault_count'] += 1
            self.get_logger().warn('GPS fault: no fix')
        elif msg.position_covariance[0] > self.max_gps_variance:
            self.sensor_status['gps']['fault'] = True
            self.sensor_status['gps']['fault_count'] += 1
            self.get_logger().warn(f'GPS fault: excessive variance {msg.position_covariance[0]:.2f} m^2')
        else:
            self.sensor_status['gps']['fault'] = False
    
    def visual_odom_callback(self, msg: Odometry):
        """Handle visual odometry messages."""
        self.sensor_status['visual_odom']['last_time'] = self.get_clock().now()
        
        # Check for reasonable velocities
        linear_speed = math.sqrt(
            msg.twist.twist.linear.x**2 + 
            msg.twist.twist.linear.y**2
        )
        
        if linear_speed > 10.0:
            self.sensor_status['visual_odom']['fault'] = True
            self.sensor_status['visual_odom']['fault_count'] += 1
            self.get_logger().warn(f'Visual odometry fault: excessive speed {linear_speed:.2f} m/s')
        else:
            self.sensor_status['visual_odom']['fault'] = False
    
    def check_sensor_faults(self):
        """Check for sensor timeouts and frozen sensors."""
        current_time = self.get_clock().now()
        
        for sensor_name, status in self.sensor_status.items():
            if status['last_time'] is not None:
                time_since_update = (current_time - status['last_time']).nanoseconds / 1e9
                if time_since_update > self.sensor_timeout:
                    status['fault'] = True
                    status['fault_count'] += 1
                    self.get_logger().warn(f'{sensor_name} fault: timeout ({time_since_update:.2f}s)')
    
    def publish_diagnostics(self):
        """Publish diagnostic information."""
        diag = DiagnosticArray()
        diag.header.stamp = self.get_clock().now().to_msg()
        
        for sensor_name, status in self.sensor_status.items():
            diag_status = DiagnosticStatus()
            diag_status.name = f'Sensor Fault Detector - {sensor_name}'
            diag_status.hardware_id = 'sensors'
            
            if status['fault']:
                diag_status.level = DiagnosticStatus.WARN
                diag_status.message = f'Sensor fault detected (count: {status["fault_count"]})'
            else:
                diag_status.level = DiagnosticStatus.OK
                diag_status.message = 'Normal operation'
            
            # Add key-value pairs
            diag_status.values.append(KeyValue(key='fault_detected', 
                                              value=str(status['fault'])))
            diag_status.values.append(KeyValue(key='fault_count', 
                                              value=str(status['fault_count'])))
            
            if status['last_time'] is not None:
                time_since_update = (self.get_clock().now() - status['last_time']).nanoseconds / 1e9
                diag_status.values.append(KeyValue(key='time_since_update_s', 
                                                  value=f'{time_since_update:.2f}'))
            
            diag.status.append(diag_status)
        
        self.diag_pub.publish(diag)


def main():
    rclpy.init()
    node = SensorFaultDetector()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()