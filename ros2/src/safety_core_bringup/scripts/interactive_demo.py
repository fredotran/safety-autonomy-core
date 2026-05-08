#!/usr/bin/env python3
"""
Interactive Safety Autonomy Core Demo

An interactive menu-driven demo that allows users to trigger specific safety scenarios on demand.
Perfect for testing individual features or demonstrating specific capabilities.
"""

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from safety_core_msgs.msg import SafetyState, EnvelopeStatus
from std_msgs.msg import Bool
import time
import math
import sys
import os
import threading

# Add demo utils to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from demo_utils import DemoLogger, MetricsDisplay


class InteractiveDemo(Node):
    def __init__(self):
        super().__init__('interactive_demo')
        
        # Initialize demo logger with colored output
        self.demo_logger = DemoLogger(self.get_logger())
        self.metrics_display = MetricsDisplay(self.demo_logger)
        
        # Publishers
        self.cmd_pub = self.create_publisher(Twist, '/cmd_vel_nav', 10)
        
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
        
        # Control flag
        self.running = True
        
        self.demo_logger.section('INTERACTIVE SAFETY AUTONOMY CORE DEMO')
        self.demo_logger.info('Select scenarios from the menu to test specific safety features', Color.CYAN)
        
        # Wait for safety system to initialize
        self.demo_logger.info('Waiting for safety system to initialize...', Color.YELLOW)
        time.sleep(2)
        
        # Start the interactive menu in a separate thread
        self.menu_thread = threading.Thread(target=self.run_menu)
        self.menu_thread.daemon = True
        self.menu_thread.start()
    
    def safety_state_callback(self, msg):
        """Track safety state changes."""
        prev_mode = self.current_mode
        prev_zone = self.current_zone
        
        self.current_mode = msg.mode
        self.fault_latched = msg.fault_latched
        self.current_zone = msg.zone.zone
        
        # Track transitions
        if prev_zone is not None and self.current_zone != prev_zone:
            self.demo_logger.track_zone_transition(prev_zone, self.current_zone)
        
        if prev_mode is not None and self.current_mode != prev_mode:
            self.demo_logger.track_mode_transition(prev_mode, self.current_mode)
        
        # Track fault and safe stop events
        if self.fault_latched and (prev_mode is None or not self.fault_latched):
            self.demo_logger.track_fault(True)
        
        # Update metrics display
        self.metrics_display.update(
            self.current_zone,
            self.current_mode,
            self.fault_latched,
            self.safe_stop_requested
        )
    
    def envelope_status_callback(self, msg):
        """Track envelope status changes."""
        self.last_envelope_status = msg
        
        # Update metrics display with envelope data
        self.metrics_display.update(
            self.current_zone,
            self.current_mode,
            self.fault_latched,
            self.safe_stop_requested,
            distance=msg.distance_to_obstacle_m,
            speed_limit=msg.recommended_speed_limit_mps
        )
    
    def safe_stop_callback(self, msg):
        """Track safe stop requests."""
        prev_safe_stop = self.safe_stop_requested
        self.safe_stop_requested = msg.data
        
        if msg.data and not prev_safe_stop:
            self.demo_logger.track_safe_stop(True)
        
        # Update metrics display
        self.metrics_display.update(
            self.current_zone,
            self.current_mode,
            self.fault_latched,
            self.safe_stop_requested
        )
    
    def stop_robot(self):
        """Stop the robot."""
        twist = Twist()
        self.cmd_pub.publish(twist)
    
    def drive_straight(self, distance, speed=None):
        """Drive straight for a given distance."""
        if speed is None:
            speed = self.linear_speed
        
        twist = Twist()
        twist.linear.x = speed
        
        duration = distance / speed
        self.demo_logger.info(f'Driving straight: {distance}m at {speed} m/s', Color.BLUE)
        
        start_time = time.time()
        while time.time() - start_time < duration and not self.fault_latched:
            self.cmd_pub.publish(twist)
            rclpy.spin_once(self, timeout_sec=0.05)
        
        self.stop_robot()
        time.sleep(0.5)
    
    def rotate(self, angle, speed=None):
        """Rotate by a given angle (radians)."""
        if speed is None:
            speed = self.angular_speed
        
        twist = Twist()
        twist.angular.z = speed if angle > 0 else -speed
        
        duration = abs(angle) / speed
        self.demo_logger.info(f'Rotating: {math.degrees(angle):.1f}° at {speed} rad/s', Color.BLUE)
        
        start_time = time.time()
        while time.time() - start_time < duration and not self.fault_latched:
            self.cmd_pub.publish(twist)
            rclpy.spin_once(self, timeout_sec=0.05)
        
        self.stop_robot()
        time.sleep(0.5)
    
    def print_menu(self):
        """Print the interactive menu."""
        print()
        print('=' * 70)
        print('INTERACTIVE SAFETY DEMO - SCENARIO MENU')
        print('=' * 70)
        print()
        print('Basic Movement:')
        print('  1. Drive forward (3m)')
        print('  2. Drive backward (2m)')
        print('  3. Rotate left (90°)')
        print('  4. Rotate right (90°)')
        print('  5. Stop robot')
        print()
        print('Safety Scenarios:')
        print('  6. Approach obstacle (trigger Warning zone)')
        print('  7. Continue approach (trigger Protective zone)')
        print('  8. Aggressive approach (trigger Emergency zone)')
        print('  9. Back away from obstacle')
        print()
        print('Sensor Failures:')
        print(' 10. Simulate lidar drop (stop publishing scan)')
        print(' 11. Resume lidar (resume publishing scan)')
        print()
        print('Command Tests:')
        print(' 12. Stop command publication (test freshness watchdog)')
        print(' 13. Resume command publication')
        print()
        print('System Control:')
        print(' 14. Show current safety state')
        print(' 15. Show metrics summary')
        print(' 16. Clear fault (if latched)')
        print()
        print('  0. Exit')
        print()
        print('=' * 70)
    
    def scenario_drive_forward(self):
        """Scenario: Drive forward."""
        self.demo_logger.subsection('Drive Forward')
        self.drive_straight(3.0)
    
    def scenario_drive_backward(self):
        """Scenario: Drive backward."""
        self.demo_logger.subsection('Drive Backward')
        self.drive_straight(-2.0)
    
    def scenario_rotate_left(self):
        """Scenario: Rotate left."""
        self.demo_logger.subsection('Rotate Left')
        self.rotate(math.pi / 2)
    
    def scenario_rotate_right(self):
        """Scenario: Rotate right."""
        self.demo_logger.subsection('Rotate Right')
        self.rotate(-math.pi / 2)
    
    def scenario_approach_warning(self):
        """Scenario: Approach obstacle to trigger Warning zone."""
        self.demo_logger.subsection('Approach Obstacle (Warning Zone)')
        self.demo_logger.info('Driving toward obstacle at moderate speed...', Color.YELLOW)
        self.rotate(math.pi / 2)  # Face toward obstacles
        self.drive_straight(1.5, speed=0.3)
        self.demo_logger.info('Check zone status - should be in Warning zone', Color.CYAN)
    
    def scenario_approach_protective(self):
        """Scenario: Continue approach to trigger Protective zone."""
        self.demo_logger.subsection('Continue Approach (Protective Zone)')
        self.demo_logger.info('Continuing approach at slower speed...', Color.YELLOW)
        self.drive_straight(0.5, speed=0.2)
        self.demo_logger.info('Check zone status - should be in Protective zone', Color.CYAN)
        self.demo_logger.info('Check mode - should be AvoidingObstacle', Color.CYAN)
    
    def scenario_approach_emergency(self):
        """Scenario: Aggressive approach to trigger Emergency zone."""
        self.demo_logger.subsection('Aggressive Approach (Emergency Zone)')
        self.demo_logger.info('Driving aggressively toward obstacle...', Color.YELLOW)
        self.drive_straight(1.0, speed=0.6)
        self.demo_logger.info('Check zone status - should be in Emergency zone', Color.CYAN)
        self.demo_logger.info('Check fault - should be latched', Color.CYAN)
    
    def scenario_back_away(self):
        """Scenario: Back away from obstacle."""
        self.demo_logger.subsection('Back Away from Obstacle')
        self.demo_logger.info('Backing away to clear zone...', Color.YELLOW)
        self.drive_straight(-2.0, speed=0.5)
        self.demo_logger.info('Robot should now be in Clear zone', Color.CYAN)
    
    def scenario_lidar_drop(self):
        """Scenario: Simulate lidar drop (not implemented - requires node control)."""
        self.demo_logger.subsection('Simulate Lidar Drop')
        self.demo_logger.warn('Lidar drop simulation requires stopping the envelope node')
        self.demo_logger.info('This scenario is for demonstration purposes only', Color.YELLOW)
        self.demo_logger.info('In production, sensor failures would trigger automatic safety responses', Color.CYAN)
    
    def scenario_resume_lidar(self):
        """Scenario: Resume lidar (not implemented - requires node control)."""
        self.demo_logger.subsection('Resume Lidar')
        self.demo_logger.warn('Lidar resume requires restarting the envelope node')
        self.demo_logger.info('This scenario is for demonstration purposes only', Color.YELLOW)
    
    def scenario_stop_commands(self):
        """Scenario: Stop command publication."""
        self.demo_logger.subsection('Stop Command Publication')
        self.demo_logger.info('Stopping command publication to test freshness watchdog...', Color.YELLOW)
        self.demo_logger.info('Commands will stop for 3 seconds', Color.CYAN)
        self.demo_logger.info('System should detect stale commands and stop the robot', Color.CYAN)
        time.sleep(3)
        self.demo_logger.info('Command publication resumed', Color.GREEN)
    
    def scenario_resume_commands(self):
        """Scenario: Resume command publication."""
        self.demo_logger.subsection('Resume Command Publication')
        self.demo_logger.info('Command publication is active', Color.GREEN)
    
    def scenario_show_state(self):
        """Scenario: Show current safety state."""
        self.demo_logger.subsection('Current Safety State')
        
        zone_names = ['Clear', 'Warning', 'Protective', 'Emergency']
        mode_names = ['Init', 'Idle', 'Moving', 'Degraded', 'AvoidingObstacle', 
                     'LocalizationLost', 'Docking', 'SafeStop']
        
        zone_name = zone_names[self.current_zone] if self.current_zone < len(zone_names) else 'Unknown'
        mode_name = mode_names[self.current_mode] if self.current_mode < len(mode_names) else 'Unknown'
        
        self.demo_logger.info(f'Current Zone: {zone_name}', Color.CYAN)
        self.demo_logger.info(f'Current Mode: {mode_name}', Color.CYAN)
        self.demo_logger.info(f'Fault Latched: {self.fault_latched}', Color.CYAN)
        self.demo_logger.info(f'Safe Stop Requested: {self.safe_stop_requested}', Color.CYAN)
        
        if self.last_envelope_status:
            self.demo_logger.info(f'Distance to Obstacle: {self.last_envelope_status.distance_to_obstacle_m:.2f}m', Color.CYAN)
            self.demo_logger.info(f'Recommended Speed Limit: {self.last_envelope_status.recommended_speed_limit_mps:.2f}m/s', Color.CYAN)
    
    def scenario_show_metrics(self):
        """Scenario: Show metrics summary."""
        self.demo_logger.subsection('Metrics Summary')
        self.demo_logger.print_metrics()
    
    def scenario_clear_fault(self):
        """Scenario: Clear fault."""
        self.demo_logger.subsection('Clear Fault')
        if not self.fault_latched:
            self.demo_logger.info('No fault is currently latched', Color.YELLOW)
            return
        
        self.demo_logger.info('Attempting to clear latched fault...', Color.YELLOW)
        self.demo_logger.info('Publishing zero commands for 3 seconds...', Color.CYAN)
        
        for _ in range(6):
            twist = Twist()
            self.cmd_pub.publish(twist)
            time.sleep(0.5)
            rclpy.spin_once(self, timeout_sec=0.1)
            
            if not self.fault_latched:
                self.demo_logger.success('Fault cleared successfully')
                return
        
        self.demo_logger.warn('Could not clear fault - may require node restart')
    
    def run_menu(self):
        """Run the interactive menu loop."""
        while self.running:
            self.print_menu()
            
            try:
                choice = input('Enter your choice (0-16): ').strip()
                
                if choice == '0':
                    self.running = False
                    self.demo_logger.section('Exiting Interactive Demo')
                    break
                elif choice == '1':
                    self.scenario_drive_forward()
                elif choice == '2':
                    self.scenario_drive_backward()
                elif choice == '3':
                    self.scenario_rotate_left()
                elif choice == '4':
                    self.scenario_rotate_right()
                elif choice == '5':
                    self.stop_robot()
                    self.demo_logger.info('Robot stopped', Color.GREEN)
                elif choice == '6':
                    self.scenario_approach_warning()
                elif choice == '7':
                    self.scenario_approach_protective()
                elif choice == '8':
                    self.scenario_approach_emergency()
                elif choice == '9':
                    self.scenario_back_away()
                elif choice == '10':
                    self.scenario_lidar_drop()
                elif choice == '11':
                    self.scenario_resume_lidar()
                elif choice == '12':
                    self.scenario_stop_commands()
                elif choice == '13':
                    self.scenario_resume_commands()
                elif choice == '14':
                    self.scenario_show_state()
                elif choice == '15':
                    self.scenario_show_metrics()
                elif choice == '16':
                    self.scenario_clear_fault()
                else:
                    self.demo_logger.error('Invalid choice. Please enter a number between 0 and 16.')
                
                time.sleep(1)
                
            except KeyboardInterrupt:
                self.running = False
                self.demo_logger.section('Exiting Interactive Demo')
                break
            except Exception as e:
                self.demo_logger.error(f'Error: {e}')
                time.sleep(1)


def main():
    rclpy.init()
    demo = InteractiveDemo()
    
    try:
        rclpy.spin(demo)
    except KeyboardInterrupt:
        pass
    finally:
        demo.stop_robot()
        demo.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
