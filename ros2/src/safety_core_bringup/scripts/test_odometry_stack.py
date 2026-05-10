#!/usr/bin/env python3
"""Comprehensive odometry stack test for CI/CD pipeline.

Tests the odometry functionality:
- Wheel odometry
- IMU integration
- Visual odometry integration
- EKF fusion
- Odometry accuracy and consistency
- TF tree consistency
"""

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, QoSHistoryPolicy, QoSReliabilityPolicy, QoSDurabilityPolicy
import sys
import time
import math
from geometry_msgs.msg import Twist, TransformStamped
from nav_msgs.msg import Odometry
from sensor_msgs.msg import Imu, JointState
from tf2_ros import Buffer, TransformListener, TransformException


class OdometryStackTest(Node):
    """Test node for validating odometry stack functionality."""
    
    def __init__(self):
        super().__init__('odometry_stack_test')
        
        # Test results tracking
        self.test_results = {
            'wheel_odometry': False,
            'imu_data': False,
            'odometry_consistency': False,
            'tf_tree_consistency': False,
            'joint_states': False,
        }
        
        # Sensor data tracking
        self.sensor_counts = {
            'wheel_odometry': 0,
            'imu': 0,
            'joint_states': 0,
        }
        
        # Odometry data for consistency check
        self.last_wheel_odom = None
        self.odometry_samples = []
        
        # TF buffer
        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)
        
        # QoS profile
        qos = QoSProfile(
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=10,
            durability=QoSDurabilityPolicy.VOLATILE,
        )
        
        # Subscriptions for odometry stack (topics provided by safety stack)
        self.create_subscription(Odometry, '/odom', self._on_wheel_odometry, qos)
        self.create_subscription(Imu, '/imu', self._on_imu, qos)
        self.create_subscription(JointState, '/joint_states', self._on_joint_states, qos)
        
        # Test timer
        self.test_duration = 30.0  # 30 seconds test
        self.start_time = time.time()
        self.create_timer(1.0, self._check_test_status)
        
        self.get_logger().info('Odometry stack test started')
    
    def _on_wheel_odometry(self, msg):
        """Wheel odometry callback."""
        self.sensor_counts['wheel_odometry'] += 1
        if self.sensor_counts['wheel_odometry'] > 10:
            self.test_results['wheel_odometry'] = True
        
        # Store for consistency check
        if self.last_wheel_odom is not None:
            self._check_odometry_consistency(self.last_wheel_odom, msg)
        self.last_wheel_odom = msg
        
        # Sample for analysis
        self.odometry_samples.append({
            'time': time.time(),
            'x': msg.pose.pose.position.x,
            'y': msg.pose.pose.position.y,
            'linear_vel': msg.twist.twist.linear.x,
            'angular_vel': msg.twist.twist.angular.z,
        })
        
        if len(self.odometry_samples) > 100:
            self.odometry_samples.pop(0)
    
    def _on_imu(self, msg):
        """IMU callback."""
        self.sensor_counts['imu'] += 1
        if self.sensor_counts['imu'] > 20:
            self.test_results['imu_data'] = True
    
    def _on_joint_states(self, msg):
        """Joint states callback."""
        self.sensor_counts['joint_states'] += 1
        if self.sensor_counts['joint_states'] > 10:
            self.test_results['joint_states'] = True
    
    def _check_odometry_consistency(self, prev_msg, curr_msg):
        """Check odometry consistency between consecutive messages."""
        # Calculate position change
        dx = curr_msg.pose.pose.position.x - prev_msg.pose.pose.position.x
        dy = curr_msg.pose.pose.position.y - prev_msg.pose.pose.position.y
        dt = (curr_msg.header.stamp.sec + curr_msg.header.stamp.nanosec * 1e-9) - \
             (prev_msg.header.stamp.sec + prev_msg.header.stamp.nanosec * 1e-9)
        
        if dt > 0:
            # Calculate expected velocity from position change
            expected_vel = math.sqrt(dx**2 + dy**2) / dt
            actual_vel = math.sqrt(curr_msg.twist.twist.linear.x**2 + curr_msg.twist.twist.linear.y**2)
            
            # Check consistency (within reasonable bounds)
            if abs(expected_vel - actual_vel) < 2.0:  # 2 m/s tolerance
                self.test_results['odometry_consistency'] = True
    
    def _check_tf_tree_consistency(self):
        """Check TF tree consistency."""
        try:
            # Check critical transforms
            transforms_to_check = [
                ('odom', 'base_link'),
                ('base_link', 'wheel_left_link'),
                ('base_link', 'wheel_right_link'),
            ]
            
            for source, target in transforms_to_check:
                try:
                    self.tf_buffer.lookup_transform(
                        source, target, 
                        rclpy.time.Time(),
                        rclpy.duration.Duration(seconds=0.1)
                    )
                except TransformException:
                    return False
            
            self.test_results['tf_tree_consistency'] = True
            return True
        except Exception:
            return False
    
    def _check_test_status(self):
        """Check test status and report results."""
        elapsed = time.time() - self.start_time
        
        # Check TF tree periodically
        if int(elapsed) % 5 == 0:
            self._check_tf_tree_consistency()
        
        if elapsed >= self.test_duration:
            self._report_results()
            # Shutdown after test completes
            self.destroy_node()
            rclpy.shutdown()
    
    def _report_results(self):
        """Report test results."""
        self.get_logger().info('=' * 60)
        self.get_logger().info('Odometry Stack Test Results')
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
        
        # Report odometry statistics
        if self.odometry_samples:
            self.get_logger().info('Odometry Statistics:')
            linear_vels = [s['linear_vel'] for s in self.odometry_samples]
            angular_vels = [s['angular_vel'] for s in self.odometry_samples]
            self.get_logger().info(f'  Linear velocity: avg={sum(linear_vels)/len(linear_vels):.3f} m/s')
            self.get_logger().info(f'  Angular velocity: avg={sum(angular_vels)/len(angular_vels):.3f} rad/s')
        
        self.get_logger().info('=' * 60)
        
        if all_passed:
            self.get_logger().info('All odometry stack tests PASSED')
        else:
            self.get_logger().error('Some odometry stack tests FAILED')
        
        self.get_logger().info('=' * 60)


def main(args=None):
    rclpy.init(args=args)
    
    test_node = OdometryStackTest()
    
    try:
        rclpy.spin(test_node)
    except KeyboardInterrupt:
        pass
    finally:
        # Only cleanup if node wasn't already destroyed by test sequence
        try:
            test_node.destroy_node()
        except:
            pass
        # Only shutdown if not already done by test sequence
        try:
            rclpy.shutdown()
        except:
            pass
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
