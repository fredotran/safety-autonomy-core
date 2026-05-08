#!/usr/bin/env python3
"""
Simple Nav2 Navigation Demo.

This script demonstrates autonomous navigation using Nav2 with the safety system.
It sets an initial pose and sends navigation goals to move the robot autonomously.
"""

import math
import time

import rclpy
from geometry_msgs.msg import PoseStamped, PoseWithCovarianceStamped
from nav2_simple_commander.robot_navigator import BasicNavigator
from rclpy.node import Node
from safety_core_msgs.msg import EnvelopeStatus, SafetyState


class Nav2Demo(Node):
    """Simple Nav2 navigation demo."""

    def __init__(self):
        """Initialize the Nav2 demo."""
        super().__init__('nav2_demo')
        
        # Initialize navigator
        self.navigator = BasicNavigator()
        
        # Subscribers for safety monitoring
        self.safety_state_sub = self.create_subscription(
            SafetyState, '/safety/state', self.safety_state_callback, 10)
        self.envelope_status_sub = self.create_subscription(
            EnvelopeStatus, '/safety/envelope_status', self.envelope_status_callback, 10)
        
        # State tracking
        self.current_mode = None
        self.current_zone = None
        self.fault_latched = False
        self.last_envelope_status = None
        
        self.get_logger().info('=' * 60)
        self.get_logger().info('NAV2 NAVIGATION DEMO')
        self.get_logger().info('=' * 60)
        
        # Wait for Nav2 to be ready
        self.get_logger().info('Waiting for Nav2 to be ready...')
        self.navigator.waitUntilNav2Active()
        self.get_logger().info('Nav2 is ready!')
        
        # Run the navigation demo
        self.run_navigation_demo()
    
    def safety_state_callback(self, msg):
        """Track safety state changes."""
        self.current_mode = msg.mode
        self.fault_latched = msg.fault_latched
        self.current_zone = msg.zone.zone
        
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
        
        self.get_logger().info(f'[Safety] Mode: {mode_name} | Zone: {zone_name} | Fault: {self.fault_latched}')
    
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
    
    def set_initial_pose(self, x, y, yaw):
        """Set the initial pose for Nav2."""
        self.get_logger().info(f'Setting initial pose: x={x}, y={y}, yaw={yaw:.2f}')
        
        initial_pose = PoseStamped()
        initial_pose.header.frame_id = 'odom'
        initial_pose.header.stamp = self.navigator.get_clock().now().to_msg()
        initial_pose.pose.position.x = x
        initial_pose.pose.position.y = y
        initial_pose.pose.position.z = 0.0
        
        # Convert yaw to quaternion
        initial_pose.pose.orientation.x = 0.0
        initial_pose.pose.orientation.y = 0.0
        initial_pose.pose.orientation.z = math.sin(yaw / 2)
        initial_pose.pose.orientation.w = math.cos(yaw / 2)
        
        self.navigator.setInitialPose(initial_pose)
        time.sleep(1)
    
    def send_goal(self, x, y, yaw):
        """Send a navigation goal."""
        self.get_logger().info(f'Sending goal: x={x}, y={y}, yaw={yaw:.2f}')
        
        goal_pose = PoseStamped()
        goal_pose.header.frame_id = 'odom'
        goal_pose.header.stamp = self.navigator.get_clock().now().to_msg()
        goal_pose.pose.position.x = x
        goal_pose.pose.position.y = y
        goal_pose.pose.position.z = 0.0
        
        # Convert yaw to quaternion
        goal_pose.pose.orientation.x = 0.0
        goal_pose.pose.orientation.y = 0.0
        goal_pose.pose.orientation.z = math.sin(yaw / 2)
        goal_pose.pose.orientation.w = math.cos(yaw / 2)
        
        # Send goal and wait for result
        self.navigator.goToPose(goal_pose)
        
        while not self.navigator.isTaskComplete():
            feedback = self.navigator.getFeedback()
            if feedback:
                self.get_logger().info(f'Distance remaining: {feedback.distance_remaining:.2f}m')
            
            # Check if fault is latched
            if self.fault_latched:
                self.get_logger().warn('Fault latched! Canceling navigation...')
                self.navigator.cancelTask()
                return False
            
            time.sleep(0.5)
        
        result = self.navigator.getResult()
        self.get_logger().info(f'Navigation result: {result}')
        return True
    
    def run_navigation_demo(self):
        """Run the navigation demo sequence."""
        self.get_logger().info('')
        self.get_logger().info('=' * 60)
        self.get_logger().info('Starting Navigation Demo')
        self.get_logger().info('=' * 60)
        
        # Set initial pose
        self.get_logger().info('')
        self.get_logger().info('Step 1: Setting initial pose at origin')
        self.set_initial_pose(0.0, 0.0, 0.0)
        time.sleep(1)
        
        # Send first goal - drive forward
        self.get_logger().info('')
        self.get_logger().info('Step 2: Navigating to first goal (3m forward)')
        success = self.send_goal(3.0, 0.0, 0.0)
        
        if not success:
            self.get_logger().warn('First goal failed or was canceled')
            return
        
        time.sleep(2)
        
        # Send second goal - rotate and drive
        self.get_logger().info('')
        self.get_logger().info('Step 3: Navigating to second goal (rotate 90 degrees)')
        success = self.send_goal(3.0, 2.0, math.pi / 2)
        
        if not success:
            self.get_logger().warn('Second goal failed or was canceled')
            return
        
        time.sleep(2)
        
        # Send third goal - return to origin
        self.get_logger().info('')
        self.get_logger().info('Step 4: Returning to origin')
        success = self.send_goal(0.0, 0.0, 0.0)
        
        if not success:
            self.get_logger().warn('Return goal failed or was canceled')
            return
        
        self.get_logger().info('')
        self.get_logger().info('=' * 60)
        self.get_logger().info('Navigation Demo Completed Successfully!')
        self.get_logger().info('=' * 60)


def main():
    rclpy.init()
    demo = Nav2Demo()
    try:
        rclpy.spin(demo)
    except KeyboardInterrupt:
        pass
    finally:
        demo.navigator.lifecycleShutdown()
        demo.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
