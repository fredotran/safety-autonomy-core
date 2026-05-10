#!/usr/bin/env python3
"""
Before/After Comparison Demo.

Demonstrates the difference between the safety system enabled and disabled:
- Without safety: Robot can collide with obstacles
- With safety: Robot stops before collisions
- Shows the value of the safety envelope system
"""

import os
import sys
import time

import rclpy
from geometry_msgs.msg import Twist
from rclpy.node import Node
from safety_core_msgs.msg import EnvelopeStatus, SafetyState
from std_msgs.msg import Bool

# Add demo utils to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from demo_utils import Color, DemoLogger


class BeforeAfterComparison(Node):
    """Before/after comparison demo for safety system."""

    def __init__(self):
        """Initialize the before/after comparison demo node."""
        super().__init__('before_after_comparison')
        
        # Initialize demo logger
        self.demo_logger = DemoLogger(self.get_logger())
        
        # Publishers
        self.cmd_pub = self.create_publisher(Twist, '/cmd_vel_nav', 10)
        self.cmd_unsafe_pub = self.create_publisher(Twist, '/cmd_vel', 10)  # Direct to Gazebo
        
        # Subscribers
        self.safety_state_sub = self.create_subscription(
            SafetyState, '/safety/state', self.safety_state_callback, 10)
        self.envelope_status_sub = self.create_subscription(
            EnvelopeStatus, '/safety/envelope_status', self.envelope_status_callback, 10)
        self.safe_stop_sub = self.create_subscription(
            Bool, '/safety/safe_stop', self.safe_stop_callback, 10)
        
        # State tracking
        self.current_mode = None
        self.current_zone = None
        self.fault_latched = False
        self.safe_stop_requested = False
        self.last_envelope_status = None
        
        # Demo parameters
        self.linear_speed = 0.8  # m/s
        self.angular_speed = 0.6  # rad/s
        
        self.demo_logger.section('BEFORE/AFTER COMPARISON DEMO')
        self.demo_logger.info('This demo compares behavior with and without safety system', Color.CYAN)
        
        # Wait for safety system to initialize
        self.demo_logger.info('Waiting for safety system to initialize...', Color.YELLOW)
        time.sleep(2)
        
        # Run comparison
        self.run_comparison()
    
    def safety_state_callback(self, msg):
        """Track safety state changes."""
        self.current_mode = msg.mode
        self.fault_latched = msg.fault_latched
        self.current_zone = msg.zone.zone
    
    def envelope_status_callback(self, msg):
        """Track envelope status changes."""
        self.last_envelope_status = msg
    
    def safe_stop_callback(self, msg):
        """Track safe stop requests."""
        self.safe_stop_requested = msg.data
    
    def stop_robot(self):
        """Stop the robot."""
        twist = Twist()
        self.cmd_pub.publish(twist)
        self.cmd_unsafe_pub.publish(twist)
    
    def drive_straight_safe(self, distance, speed=None):
        """Drive straight using safety system (cmd_vel_nav)."""
        if speed is None:
            speed = self.linear_speed
        
        twist = Twist()
        twist.linear.x = speed
        
        duration = distance / speed
        self.demo_logger.info(f'[WITH SAFETY] Driving {distance}m at {speed} m/s', Color.GREEN)
        
        start_time = time.time()
        collision_detected = False
        
        while time.time() - start_time < duration:
            self.cmd_pub.publish(twist)
            rclpy.spin_once(self, timeout_sec=0.05)
            
            # Check if safety stopped us
            if self.safe_stop_requested or self.fault_latched:
                collision_detected = True
                break
        
        self.stop_robot()
        time.sleep(0.5)
        
        return collision_detected
    
    def drive_straight_unsafe(self, distance, speed=None):
        """Drive straight bypassing safety system (cmd_vel)."""
        if speed is None:
            speed = self.linear_speed
        
        twist = Twist()
        twist.linear.x = speed
        
        duration = distance / speed
        self.demo_logger.info(f'[WITHOUT SAFETY] Driving {distance}m at {speed} m/s', Color.RED)
        
        start_time = time.time()
        collision_detected = False
        
        while time.time() - start_time < duration:
            self.cmd_unsafe_pub.publish(twist)
            rclpy.spin_once(self, timeout_sec=0.05)
            
            # Check for collision (simplified - in real Gazebo this would be physical)
            # For demo purposes, we assume collision if we reach the end without stopping
            pass
        
        self.stop_robot()
        time.sleep(0.5)
        
        return False  # Assume no collision in simulation
    
    def run_comparison(self):
        """Run the before/after comparison."""
        
        # PART 1: WITH SAFETY SYSTEM
        self.demo_logger.section('PART 1: WITH SAFETY SYSTEM ENABLED')
        self.demo_logger.info('Robot will approach obstacle with safety envelope active', Color.CYAN)
        self.demo_logger.info('Expected behavior: Robot stops before collision', Color.YELLOW)
        
        time.sleep(1)
        
        # Face toward obstacle
        self.demo_logger.info('Rotating to face obstacle...', Color.BLUE)
        twist = Twist()
        twist.angular.z = self.angular_speed
        self.cmd_pub.publish(twist)
        time.sleep(1.5)  # Rotate ~90 degrees
        self.stop_robot()
        
        # Drive toward obstacle with safety
        self.demo_logger.info('Driving toward obstacle with safety system...', Color.BLUE)
        collision = self.drive_straight_safe(3.0, speed=0.6)
        
        if collision:
            self.demo_logger.success('✓ Safety system engaged - robot stopped before collision')
        else:
            self.demo_logger.info('✓ No collision detected - obstacle not reached')
        
        # Show safety state
        self.demo_logger.subsection('Safety System Status')
        zone_names = ['Clear', 'Warning', 'Protective', 'Emergency']
        mode_names = ['Init', 'Idle', 'Moving', 'Degraded', 'AvoidingObstacle', 
                     'LocalizationLost', 'Docking', 'SafeStop']
        
        zone_name = zone_names[self.current_zone] if self.current_zone < len(zone_names) else 'Unknown'
        mode_name = mode_names[self.current_mode] if self.current_mode < len(mode_names) else 'Unknown'
        
        self.demo_logger.info(f'Final Zone: {zone_name}', Color.CYAN)
        self.demo_logger.info(f'Final Mode: {mode_name}', Color.CYAN)
        self.demo_logger.info(f'Fault Latched: {self.fault_latched}', Color.CYAN)
        
        if self.last_envelope_status:
            self.demo_logger.info(f'Final Distance to Obstacle: {self.last_envelope_status.distance_to_obstacle_m:.2f}m', Color.CYAN)
            self.demo_logger.info(f'Recommended Speed Limit: {self.last_envelope_status.recommended_speed_limit_mps:.2f}m/s', Color.CYAN)
        
        # Back away
        self.demo_logger.info('Backing away to safe position...', Color.BLUE)
        twist = Twist()
        twist.linear.x = -self.linear_speed * 0.5
        self.cmd_pub.publish(twist)
        time.sleep(2.0)
        self.stop_robot()
        
        time.sleep(2)
        
        # PART 2: WITHOUT SAFETY SYSTEM
        self.demo_logger.section('PART 2: WITHOUT SAFETY SYSTEM')
        self.demo_logger.info('Robot will approach obstacle bypassing safety envelope', Color.CYAN)
        self.demo_logger.info('Expected behavior: Robot continues until collision', Color.YELLOW)
        self.demo_logger.warn('WARNING: In simulation, collision may not be physically realistic')
        self.demo_logger.warn('This demonstrates what happens without safety protection', Color.YELLOW)
        
        time.sleep(1)
        
        # Face toward obstacle again
        self.demo_logger.info('Rotating to face obstacle...', Color.BLUE)
        twist = Twist()
        twist.angular.z = -self.angular_speed
        self.cmd_unsafe_pub.publish(twist)
        time.sleep(1.5)  # Rotate ~90 degrees
        self.stop_robot()
        
        # Drive toward obstacle without safety
        self.demo_logger.info('Driving toward obstacle WITHOUT safety system...', Color.BLUE)
        self.demo_logger.warn('Robot will not stop automatically', Color.RED)
        
        twist = Twist()
        twist.linear.x = 0.6
        self.cmd_unsafe_pub.publish(twist)
        time.sleep(3.0)  # Drive for 3 seconds
        self.stop_robot()
        
        self.demo_logger.error('✓ Without safety: Robot continued driving (collision risk)')
        
        # Summary
        self.demo_logger.section('COMPARISON SUMMARY')
        self.demo_logger.info('', Color.GREEN)
        self.demo_logger.success('WITH SAFETY SYSTEM:')
        self.demo_logger.info('- Robot detects obstacles via safety envelope', Color.CYAN)
        self.demo_logger.info('- Automatically stops before collision', Color.CYAN)
        self.demo_logger.info('- Respects speed limits in danger zones', Color.CYAN)
        self.demo_logger.info('- Latches faults on critical conditions', Color.CYAN)
        self.demo_logger.info('', Color.CYAN)
        self.demo_logger.error('WITHOUT SAFETY SYSTEM:')
        self.demo_logger.info('- No obstacle detection or avoidance', Color.CYAN)
        self.demo_logger.info('- No automatic stopping', Color.CYAN)
        self.demo_logger.info('- No speed limiting', Color.CYAN)
        self.demo_logger.info('- High collision risk', Color.CYAN)
        self.demo_logger.info('', Color.CYAN)
        self.demo_logger.success('CONCLUSION: Safety system prevents collisions and ensures safe operation')
        
        # Return to safe position
        self.demo_logger.info('Returning to safe position...', Color.BLUE)
        twist = Twist()
        twist.linear.x = -self.linear_speed * 0.5
        self.cmd_unsafe_pub.publish(twist)
        time.sleep(2.0)
        self.stop_robot()


def main():
    rclpy.init()
    comparison = BeforeAfterComparison()
    
    try:
        rclpy.spin(comparison)
    except KeyboardInterrupt:
        pass
    finally:
        comparison.stop_robot()
        comparison.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
