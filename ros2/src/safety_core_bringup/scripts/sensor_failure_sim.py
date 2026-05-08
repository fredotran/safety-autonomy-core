#!/usr/bin/env python3
"""
Sensor Failure Simulation Node.

Simulates realistic sensor failures for testing safety systems:
- Noise injection (Gaussian noise on sensor readings)
- Partial dropout (random missing readings)
- Latency injection (delayed sensor readings)
- Complete failure (stop publishing)
- Intermittent failures (periodic issues)
"""

import os
import random
import sys
import time

import rclpy
from nav_msgs.msg import Odometry
from rclpy.node import Node
from sensor_msgs.msg import Imu, LaserScan

# Add demo utils to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from demo_utils import Color, DemoLogger


class SensorFailureSim(Node):
    """Sensor failure simulation node for testing safety systems."""

    def __init__(self):
        """Initialize the sensor failure simulation node."""
        super().__init__('sensor_failure_sim')
        
        # Initialize demo logger
        self.demo_logger = DemoLogger(self.get_logger())
        
        # Publishers (republish modified sensor data)
        self.scan_pub = self.create_publisher(LaserScan, '/scan', 10)
        self.imu_pub = self.create_publisher(Imu, '/imu', 10)
        self.odom_pub = self.create_publisher(Odometry, '/odom', 10)
        
        # Subscribers (subscribe to original sensor data)
        self.scan_sub = self.create_subscription(
            LaserScan, '/scan_orig', self.scan_callback, 10)
        self.imu_sub = self.create_subscription(
            Imu, '/imu_orig', self.imu_callback, 10)
        self.odom_sub = self.create_subscription(
            Odometry, '/odom_orig', self.odom_callback, 10)
        
        # Failure modes
        self.noise_enabled = False
        self.noise_level = 0.05  # 5% noise
        self.partial_dropout_enabled = False
        self.dropout_rate = 0.1  # 10% dropout
        self.latency_enabled = False
        self.latency_ms = 100  # 100ms latency
        self.complete_failure = False
        
        # Latency buffers
        self.scan_buffer = []
        self.imu_buffer = []
        self.odom_buffer = []
        
        self.demo_logger.section('SENSOR FAILURE SIMULATION NODE')
        self.demo_logger.info('This node simulates realistic sensor failures for safety testing', Color.CYAN)
        self.demo_logger.info('Use the interactive menu to enable/disable failure modes', Color.YELLOW)
        self.demo_logger.info('', Color.CYAN)
        self.demo_logger.info('Available failure modes:', Color.CYAN)
        self.demo_logger.info('- Noise injection (Gaussian noise on readings)', Color.BLUE)
        self.demo_logger.info('- Partial dropout (random missing readings)', Color.BLUE)
        self.demo_logger.info('- Latency injection (delayed readings)', Color.BLUE)
        self.demo_logger.info('- Complete failure (stop publishing)', Color.BLUE)
        self.demo_logger.info('- Intermittent failures (periodic issues)', Color.BLUE)
        
        # Start latency processing thread
        self.latency_thread = None
        if self.latency_enabled:
            self.start_latency_processing()
    
    def scan_callback(self, msg):
        """Process laser scan with failure simulation."""
        if self.complete_failure:
            return  # Don't publish
        
        # Check for partial dropout
        if self.partial_dropout_enabled and random.random() < self.dropout_rate:
            return  # Drop this message
        
        # Apply noise
        modified_msg = LaserScan()
        modified_msg.header = msg.header
        modified_msg.angle_min = msg.angle_min
        modified_msg.angle_max = msg.angle_max
        modified_msg.angle_increment = msg.angle_increment
        modified_msg.time_increment = msg.time_increment
        modified_msg.scan_time = msg.scan_time
        modified_msg.range_min = msg.range_min
        modified_msg.range_max = msg.range_max
        
        # Apply noise to ranges
        modified_msg.ranges = []
        for r in msg.ranges:
            if self.noise_enabled:
                noise = random.gauss(0, self.noise_level * r)
                r_noisy = r + noise
                # Clamp to valid range
                r_noisy = max(msg.range_min, min(msg.range_max, r_noisy))
                modified_msg.ranges.append(r_noisy)
            else:
                modified_msg.ranges.append(r)
        
        # Apply latency if enabled
        if self.latency_enabled:
            self.scan_buffer.append((time.time(), modified_msg))
        else:
            self.scan_pub.publish(modified_msg)
    
    def imu_callback(self, msg):
        """Process IMU with failure simulation."""
        if self.complete_failure:
            return
        
        # Check for partial dropout
        if self.partial_dropout_enabled and random.random() < self.dropout_rate:
            return
        
        # Apply noise
        modified_msg = Imu()
        modified_msg.header = msg.header
        
        if self.noise_enabled:
            # Add noise to angular velocity
            modified_msg.angular_velocity.x = msg.angular_velocity.x + random.gauss(0, 0.01)
            modified_msg.angular_velocity.y = msg.angular_velocity.y + random.gauss(0, 0.01)
            modified_msg.angular_velocity.z = msg.angular_velocity.z + random.gauss(0, 0.01)
            
            # Add noise to linear acceleration
            modified_msg.linear_acceleration.x = msg.linear_acceleration.x + random.gauss(0, 0.1)
            modified_msg.linear_acceleration.y = msg.linear_acceleration.y + random.gauss(0, 0.1)
            modified_msg.linear_acceleration.z = msg.linear_acceleration.z + random.gauss(0, 0.1)
            
            # Add noise to orientation
            modified_msg.orientation.x = msg.orientation.x + random.gauss(0, 0.001)
            modified_msg.orientation.y = msg.orientation.y + random.gauss(0, 0.001)
            modified_msg.orientation.z = msg.orientation.z + random.gauss(0, 0.001)
            modified_msg.orientation.w = msg.orientation.w + random.gauss(0, 0.001)
        else:
            modified_msg.angular_velocity = msg.angular_velocity
            modified_msg.linear_acceleration = msg.linear_acceleration
            modified_msg.orientation = msg.orientation
        
        # Apply latency if enabled
        if self.latency_enabled:
            self.imu_buffer.append((time.time(), modified_msg))
        else:
            self.imu_pub.publish(modified_msg)
    
    def odom_callback(self, msg):
        """Process odometry with failure simulation."""
        if self.complete_failure:
            return
        
        # Check for partial dropout
        if self.partial_dropout_enabled and random.random() < self.dropout_rate:
            return
        
        # Apply noise (minimal for odometry)
        modified_msg = Odometry()
        modified_msg.header = msg.header
        modified_msg.child_frame_id = msg.child_frame_id
        
        if self.noise_enabled:
            # Add small noise to pose
            modified_msg.pose.pose.position.x = msg.pose.pose.position.x + random.gauss(0, 0.001)
            modified_msg.pose.pose.position.y = msg.pose.pose.position.y + random.gauss(0, 0.001)
            modified_msg.pose.pose.position.z = msg.pose.pose.position.z + random.gauss(0, 0.001)
            modified_msg.pose.pose.orientation = msg.pose.pose.orientation
            
            # Add noise to twist
            modified_msg.twist.twist.linear.x = msg.twist.twist.linear.x + random.gauss(0, 0.01)
            modified_msg.twist.twist.linear.y = msg.twist.twist.linear.y + random.gauss(0, 0.01)
            modified_msg.twist.twist.linear.z = msg.twist.twist.linear.z + random.gauss(0, 0.01)
            modified_msg.twist.twist.angular = msg.twist.twist.angular
        else:
            modified_msg.pose = msg.pose
            modified_msg.twist = msg.twist
        
        # Apply latency if enabled
        if self.latency_enabled:
            self.odom_buffer.append((time.time(), modified_msg))
        else:
            self.odom_pub.publish(modified_msg)
    
    def start_latency_processing(self):
        """Start thread to process latency buffers."""
        self.latency_thread = threading.Thread(target=self.process_latency)
        self.latency_thread.daemon = True
        self.latency_thread.start()
    
    def process_latency(self):
        """Process messages with latency."""
        while rclpy.ok():
            current_time = time.time()
            
            # Process scan buffer
            while self.scan_buffer and current_time - self.scan_buffer[0][0] >= self.latency_ms / 1000.0:
                _, msg = self.scan_buffer.pop(0)
                self.scan_pub.publish(msg)
            
            # Process IMU buffer
            while self.imu_buffer and current_time - self.imu_buffer[0][0] >= self.latency_ms / 1000.0:
                _, msg = self.imu_buffer.pop(0)
                self.imu_pub.publish(msg)
            
            # Process odom buffer
            while self.odom_buffer and current_time - self.odom_buffer[0][0] >= self.latency_ms / 1000.0:
                _, msg = self.odom_buffer.pop(0)
                self.odom_pub.publish(msg)
            
            time.sleep(0.01)
    
    def enable_noise(self, level=0.05):
        """Enable noise injection."""
        self.noise_enabled = True
        self.noise_level = level
        self.demo_logger.success(f'Noise injection enabled (level: {level*100}%)')
    
    def disable_noise(self):
        """Disable noise injection."""
        self.noise_enabled = False
        self.demo_logger.info('Noise injection disabled', Color.YELLOW)
    
    def enable_partial_dropout(self, rate=0.1):
        """Enable partial dropout."""
        self.partial_dropout_enabled = True
        self.dropout_rate = rate
        self.demo_logger.success(f'Partial dropout enabled (rate: {rate*100}%)')
    
    def disable_partial_dropout(self):
        """Disable partial dropout."""
        self.partial_dropout_enabled = False
        self.demo_logger.info('Partial dropout disabled', Color.YELLOW)
    
    def enable_latency(self, latency_ms=100):
        """Enable latency injection."""
        self.latency_enabled = True
        self.latency_ms = latency_ms
        if self.latency_thread is None:
            self.start_latency_processing()
        self.demo_logger.success(f'Latency injection enabled (latency: {latency_ms}ms)')
    
    def disable_latency(self):
        """Disable latency injection."""
        self.latency_enabled = False
        # Flush buffers
        while self.scan_buffer:
            _, msg = self.scan_buffer.pop(0)
            self.scan_pub.publish(msg)
        while self.imu_buffer:
            _, msg = self.imu_buffer.pop(0)
            self.imu_pub.publish(msg)
        while self.odom_buffer:
            _, msg = self.odom_buffer.pop(0)
            self.odom_pub.publish(msg)
        self.demo_logger.info('Latency injection disabled', Color.YELLOW)
    
    def enable_complete_failure(self):
        """Enable complete sensor failure."""
        self.complete_failure = True
        self.demo_logger.error('Complete sensor failure enabled - all sensors stopped')
    
    def disable_complete_failure(self):
        """Disable complete sensor failure."""
        self.complete_failure = False
        self.demo_logger.success('Complete sensor failure disabled - sensors resumed')
    
    def get_status(self):
        """Get current failure simulation status."""
        status = {
            'noise_enabled': self.noise_enabled,
            'noise_level': self.noise_level,
            'partial_dropout_enabled': self.partial_dropout_enabled,
            'dropout_rate': self.dropout_rate,
            'latency_enabled': self.latency_enabled,
            'latency_ms': self.latency_ms,
            'complete_failure': self.complete_failure
        }
        return status


def main():
    rclpy.init()
    
    # Check if we want interactive mode or simple mode
    if len(sys.argv) > 1 and sys.argv[1] == '--interactive':
        # Interactive mode with menu
        sim = SensorFailureSim()
        
        def run_menu():
            while rclpy.ok():
                print()
                print('=' * 70)
                print('SENSOR FAILURE SIMULATION - INTERACTIVE MENU')
                print('=' * 70)
                print()
                print('Failure Modes:')
                print('  1. Enable noise injection (5%)')
                print('  2. Disable noise injection')
                print('  3. Enable partial dropout (10%)')
                print('  4. Disable partial dropout')
                print('  5. Enable latency injection (100ms)')
                print('  6. Disable latency injection')
                print('  7. Enable complete failure')
                print('  8. Disable complete failure')
                print()
                print('Status:')
                print('  9. Show current status')
                print()
                print('  0. Exit')
                print()
                print('=' * 70)
                
                try:
                    choice = input('Enter your choice (0-9): ').strip()
                    
                    if choice == '0':
                        sim.demo_logger.section('Exiting Sensor Failure Simulation')
                        break
                    elif choice == '1':
                        sim.enable_noise()
                    elif choice == '2':
                        sim.disable_noise()
                    elif choice == '3':
                        sim.enable_partial_dropout()
                    elif choice == '4':
                        sim.disable_partial_dropout()
                    elif choice == '5':
                        sim.enable_latency()
                    elif choice == '6':
                        sim.disable_latency()
                    elif choice == '7':
                        sim.enable_complete_failure()
                    elif choice == '8':
                        sim.disable_complete_failure()
                    elif choice == '9':
                        status = sim.get_status()
                        sim.demo_logger.subsection('Current Status')
                        sim.demo_logger.info(f'Noise enabled: {status["noise_enabled"]}', Color.CYAN)
                        sim.demo_logger.info(f'Noise level: {status["noise_level"]*100}%', Color.CYAN)
                        sim.demo_logger.info(f'Partial dropout enabled: {status["partial_dropout_enabled"]}', Color.CYAN)
                        sim.demo_logger.info(f'Dropout rate: {status["dropout_rate"]*100}%', Color.CYAN)
                        sim.demo_logger.info(f'Latency enabled: {status["latency_enabled"]}', Color.CYAN)
                        sim.demo_logger.info(f'Latency: {status["latency_ms"]}ms', Color.CYAN)
                        sim.demo_logger.info(f'Complete failure: {status["complete_failure"]}', Color.CYAN)
                    else:
                        sim.demo_logger.error('Invalid choice')
                    
                    time.sleep(0.5)
                    
                except KeyboardInterrupt:
                    sim.demo_logger.section('Exiting Sensor Failure Simulation')
                    break
                except Exception as e:
                    sim.demo_logger.error(f'Error: {e}')
                    time.sleep(1)
        
        menu_thread = threading.Thread(target=run_menu)
        menu_thread.daemon = True
        menu_thread.start()
        
        try:
            rclpy.spin(sim)
        except KeyboardInterrupt:
            pass
        finally:
            sim.destroy_node()
            rclpy.shutdown()
    else:
        # Simple mode - just run the node without interactive menu
        sim = SensorFailureSim()
        try:
            rclpy.spin(sim)
        except KeyboardInterrupt:
            pass
        finally:
            sim.destroy_node()
            rclpy.shutdown()


if __name__ == '__main__':
    main()
