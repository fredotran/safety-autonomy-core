#!/usr/bin/env python3
"""Path pattern generator for the AGV - provides various movement patterns.

Usage:
    python3 path_patterns.py --pattern square
    python3 path_patterns.py --pattern circle
    python3 path_patterns.py --pattern figure8
    python3 path_patterns.py --pattern manual
"""

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import time
import math
import argparse


class PathPatternGenerator(Node):
    def __init__(self, pattern='square'):
        super().__init__('path_pattern_generator')
        self.cmd_pub = self.create_publisher(Twist, '/cmd_vel_nav', 10)
        
        # Movement parameters
        self.linear_speed = 0.5  # m/s
        self.angular_speed = 0.5  # rad/s
        
        self.get_logger().info(f'Path pattern generator started with pattern: {pattern}')
        self.get_logger().info(f'Linear speed: {self.linear_speed} m/s')
        self.get_logger().info(f'Angular speed: {self.angular_speed} rad/s')
        
        # Execute the requested pattern
        if pattern == 'square':
            self.square_pattern()
        elif pattern == 'circle':
            self.circle_pattern()
        elif pattern == 'figure8':
            self.figure8_pattern()
        elif pattern == 'manual':
            self.manual_control()
        else:
            self.get_logger().error(f'Unknown pattern: {pattern}')
    
    def stop(self):
        """Stop the robot."""
        twist = Twist()
        self.cmd_pub.publish(twist)
    
    def drive_straight(self, distance):
        """Drive straight for a given distance."""
        twist = Twist()
        twist.linear.x = self.linear_speed
        
        duration = distance / self.linear_speed
        self.get_logger().info(f'Driving straight for {distance}m (duration: {duration:.2f}s)')
        
        start_time = time.time()
        while time.time() - start_time < duration:
            self.cmd_pub.publish(twist)
            rclpy.spin_once(self, timeout_sec=0.1)
        
        self.stop()
        time.sleep(0.5)
    
    def rotate(self, angle):
        """Rotate by a given angle (radians)."""
        twist = Twist()
        twist.angular.z = self.angular_speed if angle > 0 else -self.angular_speed
        
        duration = abs(angle) / self.angular_speed
        self.get_logger().info(f'Rotating {math.degrees(angle):.1f} degrees (duration: {duration:.2f}s)')
        
        start_time = time.time()
        while time.time() - start_time < duration:
            self.cmd_pub.publish(twist)
            rclpy.spin_once(self, timeout_sec=0.1)
        
        self.stop()
        time.sleep(0.5)
    
    def square_pattern(self, side_length=2.0, repetitions=2):
        """Drive in a square pattern."""
        self.get_logger().info(f'Executing square pattern (side: {side_length}m, reps: {repetitions})')
        
        for rep in range(repetitions):
            self.get_logger().info(f'--- Repetition {rep+1}/{repetitions} ---')
            for i in range(4):
                self.get_logger().info(f'Side {i+1}/4')
                self.drive_straight(side_length)
                self.rotate(math.pi / 2)  # 90 degrees
        
        self.get_logger().info('Square pattern complete!')
    
    def circle_pattern(self, radius=1.0, repetitions=1):
        """Drive in a circle pattern."""
        self.get_logger().info(f'Executing circle pattern (radius: {radius}m, reps: {repetitions})')
        
        # For a circle, we need to drive forward while turning
        # Angular velocity = linear velocity / radius
        circle_angular_speed = self.linear_speed / radius
        
        twist = Twist()
        twist.linear.x = self.linear_speed
        twist.angular.z = circle_angular_speed
        
        # Circumference = 2 * pi * radius
        circumference = 2 * math.pi * radius
        duration = circumference / self.linear_speed
        
        for rep in range(repetitions):
            self.get_logger().info(f'Circle {rep+1}/{repetitions} (duration: {duration:.2f}s)')
            start_time = time.time()
            while time.time() - start_time < duration:
                self.cmd_pub.publish(twist)
                rclpy.spin_once(self, timeout_sec=0.1)
            self.stop()
            time.sleep(0.5)
        
        self.get_logger().info('Circle pattern complete!')
    
    def figure8_pattern(self, radius=1.0, repetitions=1):
        """Drive in a figure-8 pattern."""
        self.get_logger().info(f'Executing figure-8 pattern (radius: {radius}m, reps: {repetitions})')
        
        # Figure-8 is two circles in opposite directions
        circle_angular_speed = self.linear_speed / radius
        circumference = 2 * math.pi * radius
        circle_duration = circumference / self.linear_speed
        
        for rep in range(repetitions):
            self.get_logger().info(f'Figure-8 {rep+1}/{repetitions}')
            
            # First circle (clockwise)
            self.get_logger().info('First circle (clockwise)')
            twist = Twist()
            twist.linear.x = self.linear_speed
            twist.angular.z = circle_angular_speed
            
            start_time = time.time()
            while time.time() - start_time < circle_duration:
                self.cmd_pub.publish(twist)
                rclpy.spin_once(self, timeout_sec=0.1)
            
            self.stop()
            time.sleep(0.5)
            
            # Second circle (counter-clockwise)
            self.get_logger().info('Second circle (counter-clockwise)')
            twist.angular.z = -circle_angular_speed
            
            start_time = time.time()
            while time.time() - start_time < circle_duration:
                self.cmd_pub.publish(twist)
                rclpy.spin_once(self, timeout_sec=0.1)
            
            self.stop()
            time.sleep(0.5)
        
        self.get_logger().info('Figure-8 pattern complete!')
    
    def manual_control(self):
        """Manual control mode - drive forward, then turn, then back."""
        self.get_logger().info('Manual control pattern')
        
        # Drive forward
        self.get_logger().info('Driving forward...')
        self.drive_straight(3.0)
        
        # Turn around
        self.get_logger().info('Turning around...')
        self.rotate(math.pi)  # 180 degrees
        
        # Drive back
        self.get_logger().info('Driving back...')
        self.drive_straight(3.0)
        
        self.get_logger().info('Manual control pattern complete!')


def main():
    parser = argparse.ArgumentParser(description='Path pattern generator for AGV')
    parser.add_argument('--pattern', type=str, default='square',
                        choices=['square', 'circle', 'figure8', 'manual'],
                        help='Movement pattern to execute')
    parser.add_argument('--reps', type=int, default=2,
                        help='Number of repetitions')
    
    args = parser.parse_args()
    
    rclpy.init()
    path_generator = PathPatternGenerator(pattern=args.pattern)
    
    try:
        rclpy.spin(path_generator)
    except KeyboardInterrupt:
        path_generator.get_logger().info('Path generator stopped by user')
    finally:
        path_generator.stop()
        path_generator.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
