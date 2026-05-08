#!/usr/bin/env python3
"""Simple path follower for the AGV - makes the robot drive in a square pattern."""

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import time
import math


class PathFollower(Node):
    def __init__(self):
        super().__init__('path_follower')
        self.cmd_pub = self.create_publisher(Twist, '/cmd_vel_nav', 10)
        
        # Movement parameters
        self.linear_speed = 0.5  # m/s
        self.angular_speed = 0.5  # rad/s
        self.side_length = 2.0  # meters per side of square
        self.rotation_angle = math.pi / 2  # 90 degrees in radians
        
        self.get_logger().info('Path follower started')
        self.get_logger().info(f'Linear speed: {self.linear_speed} m/s')
        self.get_logger().info(f'Angular speed: {self.angular_speed} rad/s')
        self.get_logger().info(f'Square side length: {self.side_length} m')
        
        # Start the path following
        self.follow_square_path()
    
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
        
        # Stop
        twist.linear.x = 0.0
        self.cmd_pub.publish(twist)
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
        
        # Stop
        twist.angular.z = 0.0
        self.cmd_pub.publish(twist)
        time.sleep(0.5)
    
    def follow_square_path(self):
        """Follow a square path: 4 sides, 4 turns."""
        self.get_logger().info('Starting square path...')
        
        for i in range(4):
            self.get_logger().info(f'--- Side {i+1}/4 ---')
            self.drive_straight(self.side_length)
            self.rotate(self.rotation_angle)
        
        self.get_logger().info('Square path complete!')
        
        # Optional: repeat the path
        self.get_logger().info('Repeating path...')
        for i in range(4):
            self.get_logger().info(f'--- Side {i+1}/4 ---')
            self.drive_straight(self.side_length)
            self.rotate(self.rotation_angle)
        
        self.get_logger().info('Path following complete!')


def main():
    rclpy.init()
    path_follower = PathFollower()
    
    try:
        rclpy.spin(path_follower)
    except KeyboardInterrupt:
        path_follower.get_logger().info('Path follower stopped by user')
    finally:
        path_follower.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
