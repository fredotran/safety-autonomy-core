#!/usr/bin/env python3
"""
Quick Safety Autonomy Core Demo.

A fast-paced 1-2 minute demo covering core safety features:
1. Initial state and normal operation
2. Speed limiting in Warning zone
3. Protective zone and obstacle avoidance
4. Emergency zone and emergency stop
5. Fault recovery
"""

import math
import time

import rclpy
from geometry_msgs.msg import Twist
from rclpy.node import Node
from safety_core_msgs.msg import EnvelopeStatus, SafetyState
from std_msgs.msg import Bool


class QuickDemo(Node):
    """Quick demo for safety autonomy core system."""

    def __init__(self):
        """Initialize the quick demo node."""
        super().__init__('quick_demo')
        
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
        
        # Demo parameters (faster for quick demo)
        self.linear_speed = 1.0  # m/s
        self.angular_speed = 0.8  # rad/s
        
        # Metrics tracking
        self.zone_transitions = []
        self.last_zone = None
        self.start_time = time.time()
        
        # Demo execution timer
        self.demo_timer = self.create_timer(3.0, self.run_demo_sequence)
        
        self.get_logger().info('=' * 60)
        self.get_logger().info('QUICK SAFETY AUTONOMY CORE DEMO')
        self.get_logger().info('=' * 60)
        self.get_logger().info('')
        self.get_logger().info('Waiting for safety system to initialize...')
    
    def safety_state_callback(self, msg):
        """Track safety state changes."""
        self.current_mode = msg.mode
        self.fault_latched = msg.fault_latched
        self.current_zone = msg.zone.zone
        
        # Track zone transitions
        if self.last_zone is not None and self.current_zone != self.last_zone:
            transition_time = time.time() - self.start_time
            zone_names = ['Clear', 'Warning', 'Protective', 'Emergency']
            self.zone_transitions.append({
                'from': zone_names[self.last_zone],
                'to': zone_names[self.current_zone],
                'time': transition_time
            })
        self.last_zone = self.current_zone
        
        mode_names = {
            0: 'Init',
            1: 'Idle',
            2: 'Moving',
            3: 'Degraded',
            4: 'AvoidingObstacle',
            5: 'LocalizationLost',
            6: 'Docking',
            7: 'SafeStop'
        }
        
        zone_names = {
            0: 'Clear',
            1: 'Warning',
            2: 'Protective',
            3: 'Emergency'
        }
        
        mode_name = mode_names.get(self.current_mode, f'Unknown({self.current_mode})')
        zone_name = zone_names.get(self.current_zone, f'Unknown({self.current_zone})')
        
        self.get_logger().info(f'[State] Mode: {mode_name} | Zone: {zone_name} | Fault: {self.fault_latched}')
    
    def envelope_status_callback(self, msg):
        """Track envelope status changes."""
        self.last_envelope_status = msg
        
        zone_names = {
            0: 'Clear',
            1: 'Warning',
            2: 'Protective',
            3: 'Emergency'
        }
        zone_name = zone_names.get(msg.zone.zone, 'Unknown')
        
        self.get_logger().info(f'[Envelope] Zone: {zone_name} | Distance: {msg.distance_to_obstacle_m:.2f}m | '
                            f'Speed Limit: {msg.recommended_speed_limit_mps:.2f}m/s')
    
    def safe_stop_callback(self, msg):
        """Track safe stop requests."""
        self.safe_stop_requested = msg.data
        if msg.data:
            self.get_logger().warn('[SafeStop] REQUESTED')
    
    def run_demo_sequence(self):
        """Run the quick demo sequence (called by timer after initialization)."""
        self.demo_timer.cancel()
        self.get_logger().info('Starting demo sequence...')
        
        # Run demo sequence with small delays to allow callback processing
        try:
            # Section 1: Initial State and Normal Operation
            self.print_section('Section 1: Initial State and Normal Operation')
            self.get_logger().info('Verifying initial safety state...')
            
            if self.current_mode is not None:
                mode_names = {0: 'Init', 1: 'Idle', 2: 'Moving', 3: 'Degraded', 4: 'AvoidingObstacle', 
                            5: 'LocalizationLost', 6: 'Docking', 7: 'SafeStop'}
                mode_name = mode_names.get(self.current_mode, f'Unknown({self.current_mode})')
                zone_names = {0: 'Clear', 1: 'Warning', 2: 'Protective', 3: 'Emergency'}
                zone_name = zone_names.get(self.current_zone, 'Unknown') if self.current_zone is not None else 'Unknown'
                
                self.get_logger().info(f'Initial Mode: {mode_name}')
                self.get_logger().info(f'Initial Zone: {zone_name}')
                self.get_logger().info(f'Fault Latched: {self.fault_latched}')
            
            self.get_logger().info('Driving forward in Clear zone...')
            self.publish_velocity(self.linear_speed, 0.0)
            time.sleep(2.0)
            self.stop_robot()
            time.sleep(0.5)
            
            # Section 2: Speed Limiting in Warning Zone
            self.print_section('Section 2: Speed Limiting in Warning Zone')
            self.get_logger().info('Driving toward obstacles to trigger Warning zone...')
            self.publish_velocity(self.linear_speed, 0.0)
            time.sleep(3.0)
            self.stop_robot()
            
            if self.last_envelope_status:
                self.get_logger().info(f'Recommended speed limit: {self.last_envelope_status.recommended_speed_limit_mps:.2f} m/s')
            
            time.sleep(0.5)
            
            # Section 3: Protective Zone and Obstacle Avoidance
            self.print_section('Section 3: Protective Zone and Obstacle Avoidance')
            self.get_logger().info('Continuing approach to trigger Protective zone...')
            self.publish_velocity(self.linear_speed * 0.5, 0.0)
            time.sleep(2.0)
            self.stop_robot()
            
            if self.current_mode == 4:  # AvoidingObstacle
                self.get_logger().info('Successfully entered AvoidingObstacle mode')
            
            time.sleep(0.5)
            
            # Section 4: Emergency Zone and Emergency Stop
            self.print_section('Section 4: Emergency Zone and Emergency Stop')
            self.get_logger().info('Aggressive approach to trigger Emergency zone...')
            self.publish_velocity(self.linear_speed * 0.8, 0.0)
            time.sleep(1.5)
            self.stop_robot()
            
            if self.current_zone == 3:  # Emergency
                self.get_logger().info('Successfully entered Emergency zone')
            
            if self.fault_latched:
                self.get_logger().info('Fault successfully latched')
            
            time.sleep(1.0)
            
            # Section 5: Fault Recovery
            self.print_section('Section 5: Fault Recovery')
            self.get_logger().info('Backing away to clear zone...')
            self.publish_velocity(-self.linear_speed * 0.5, 0.0)
            time.sleep(2.0)
            self.stop_robot()
            
            self.get_logger().info('Attempting to drive with latched fault (should fail)...')
            self.publish_velocity(self.linear_speed, 0.0)
            time.sleep(1.0)
            self.stop_robot()
            
            self.get_logger().info('Clearing fault (requires safety node restart in production)')
            self.get_logger().info('For demo purposes, we will demonstrate recovery by backing away')
            
            time.sleep(0.5)
            
            # Summary
            self.print_section('Demo Summary')
            total_time = time.time() - self.start_time
            self.get_logger().info(f'Total demo duration: {total_time:.1f} seconds')
            self.get_logger().info(f'Zone transitions: {len(self.zone_transitions)}')
            
            if self.zone_transitions:
                self.get_logger().info('Transition history:')
                for i, transition in enumerate(self.zone_transitions, 1):
                    self.get_logger().info(f'  {i}. {transition["from"]} -> {transition["to"]} at {transition["time"]:.1f}s')
            
            self.get_logger().info('')
            self.get_logger().info('Quick demo completed successfully!')
            self.get_logger().info('For comprehensive edge cases, run comprehensive_demo.py')
            
            # Shutdown
            self.stop_robot()
            time.sleep(0.5)
            
            # Trigger shutdown
            self.get_logger().info('Demo completed, shutting down...')
            self.destroy_node()
            rclpy.shutdown()
            
        except Exception as e:
            self.get_logger().error(f'Demo failed with exception: {e}')
            self.stop_robot()
            self.destroy_node()
            rclpy.shutdown()
    
    def publish_velocity(self, linear_x, angular_z):
        """Publish velocity command."""
        cmd = Twist()
        cmd.linear.x = linear_x
        cmd.angular.z = angular_z
        self.cmd_pub.publish(cmd)
    
    def stop_robot(self):
        """Stop the robot."""
        self.publish_velocity(0.0, 0.0)
    
    def print_section(self, title):
        """Print a section header."""
        self.get_logger().info('')
        self.get_logger().info('=' * 60)
        self.get_logger().info(f'  {title}')
        self.get_logger().info('=' * 60)


def main():
    rclpy.init()
    demo = QuickDemo()
    
    try:
        rclpy.spin(demo)
    except KeyboardInterrupt:
        demo.get_logger().info('Demo interrupted by user')
    except Exception as e:
        demo.get_logger().error(f'Demo failed with exception: {e}')
    finally:
        # Only cleanup if node wasn't already destroyed by demo sequence
        try:
            demo.stop_robot()
            demo.destroy_node()
        except:
            pass
        # Only shutdown if not already done by demo sequence
        try:
            rclpy.shutdown()
        except:
            pass


if __name__ == '__main__':
    main()
