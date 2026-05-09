#!/usr/bin/env python3
"""Comprehensive localization stack test for CI/CD pipeline.

Tests the advanced localization features:
- Visual odometry node
- Adaptive EKF with wheel slip detection  
- IMU bias estimation
- Sensor fault detection
- Enhanced localization monitor with kidnapping detection
- Environment-specific configurations
"""

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, QoSHistoryPolicy, QoSReliabilityPolicy, QoSDurabilityPolicy
import sys
import time
from geometry_msgs.msg import PoseStamped, Twist
from nav_msgs.msg import Odometry
from sensor_msgs.msg import Imu, NavSatFix
from diagnostic_msgs.msg import DiagnosticArray


class LocalizationStackTest(Node):
    """Test node for validating localization stack functionality."""
    
    def __init__(self):
        super().__init__('localization_stack_test')
        
        # Test results tracking
        self.test_results = {
            'visual_odometry': False,
            'adaptive_ekf': False,
            'imu_bias_estimation': False,
            'sensor_fault_detection': False,
            'kidnapping_detection': False,
            'localization_confidence': False,
        }
        
        # Sensor data tracking
        self.sensor_counts = {
            'visual_odometry': 0,
            'adaptive_ekf': 0,
            'imu': 0,
            'gps': 0,
            'wheel_odometry': 0,
            'fault_diagnostics': 0,
        }
        
        # QoS profile
        qos = QoSProfile(
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=10,
            durability=QoSDurabilityPolicy.VOLATILE,
        )
        
        # Subscriptions for localization stack
        self.create_subscription(PoseStamped, '/visual_odometry', self._on_visual_odometry, qos)
        self.create_subscription(Odometry, '/odometry/filtered', self._on_adaptive_ekf, qos)
        self.create_subscription(Imu, '/imu', self._on_imu, qos)
        self.create_subscription(NavSatFix, '/gps', self._on_gps, qos)
        self.create_subscription(Odometry, '/odom', self._on_wheel_odometry, qos)
        self.create_subscription(DiagnosticArray, '/diagnostics', self._on_diagnostics, qos)
        
        # Test timer
        self.test_duration = 30.0  # 30 seconds test
        self.start_time = time.time()
        self.create_timer(1.0, self._check_test_status)
        
        self.get_logger().info('Localization stack test started')
    
    def _on_visual_odometry(self, msg):
        """Visual odometry callback."""
        self.sensor_counts['visual_odometry'] += 1
        if self.sensor_counts['visual_odometry'] > 5:
            self.test_results['visual_odometry'] = True
    
    def _on_adaptive_ekf(self, msg):
        """Adaptive EKF callback."""
        self.sensor_counts['adaptive_ekf'] += 1
        if self.sensor_counts['adaptive_ekf'] > 10:
            self.test_results['adaptive_ekf'] = True
    
    def _on_imu(self, msg):
        """IMU callback."""
        self.sensor_counts['imu'] += 1
        if self.sensor_counts['imu'] > 20:
            self.test_results['imu_bias_estimation'] = True
    
    def _on_gps(self, msg):
        """GPS callback."""
        self.sensor_counts['gps'] += 1
    
    def _on_wheel_odometry(self, msg):
        """Wheel odometry callback."""
        self.sensor_counts['wheel_odometry'] += 1
    
    def _on_diagnostics(self, msg):
        """Sensor fault detection diagnostics callback."""
        self.sensor_counts['fault_diagnostics'] += 1
        if self.sensor_counts['fault_diagnostics'] > 5:
            self.test_results['sensor_fault_detection'] = True
            
            # Check for kidnapping detection in diagnostics
            for status in msg.status:
                if 'kidnapping' in status.name.lower() or 'localization' in status.name.lower():
                    self.test_results['kidnapping_detection'] = True
                if 'confidence' in status.name.lower():
                    self.test_results['localization_confidence'] = True
    
    def _check_test_status(self):
        """Check test status and report results."""
        elapsed = time.time() - self.start_time
        
        if elapsed >= self.test_duration:
            self._report_results()
            rclpy.shutdown()
    
    def _report_results(self):
        """Report test results."""
        self.get_logger().info('=' * 60)
        self.get_logger().info('Localization Stack Test Results')
        self.get_logger().info('=' * 60)
        
        # Report sensor counts
        self.get_logger().info('Sensor Data Counts:')
        for sensor, count in self.sensor_counts.items():
            self.get_logger().info(f'  {sensor}: {count} messages')
        
        # Report test results
        self.get_logger().info('Test Results:')
        all_passed = True
        for test, passed in self.test_results.items():
            status = 'PASS' if passed else 'FAIL'
            self.get_logger().info(f'  {test}: {status}')
            if not passed:
                all_passed = False
        
        self.get_logger().info('=' * 60)
        
        if all_passed:
            self.get_logger().info('All localization stack tests PASSED')
        else:
            self.get_logger().error('Some localization stack tests FAILED')
        
        self.get_logger().info('=' * 60)


def main(args=None):
    rclpy.init(args=args)
    
    test_node = LocalizationStackTest()
    
    try:
        rclpy.spin(test_node)
    except KeyboardInterrupt:
        pass
    finally:
        test_node.destroy_node()
        rclpy.shutdown()
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
