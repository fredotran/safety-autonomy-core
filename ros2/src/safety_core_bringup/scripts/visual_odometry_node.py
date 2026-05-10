#!/usr/bin/env python3
"""Visual odometry node for safety_core AGV.

Uses camera data to estimate robot motion through feature tracking.
Publishes visual odometry to /visual_odometry topic for fusion with wheel odometry.

Usage:
    ros2 run safety_core_bringup visual_odometry_node.py
"""
from __future__ import annotations

import cv2
import numpy as np
import rclpy
from cv_bridge import CvBridge
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import QoSDurabilityPolicy, QoSHistoryPolicy, QoSProfile, QoSReliabilityPolicy
from sensor_msgs.msg import CameraInfo, Image


class VisualOdometryNode(Node):
    """Visual odometry node using feature tracking."""
    
    def __init__(self):
        super().__init__('visual_odometry_node')
        
        # Parameters
        self.max_features = self.declare_parameter('max_features', 500).value
        self.min_matches = self.declare_parameter('min_matches', 10).value
        self.min_distance = self.declare_parameter('min_distance', 30.0).value
        self.scale_factor = self.declare_parameter('scale_factor', 0.01).value  # Pixels to meters conversion
        
        # Camera calibration
        self.camera_matrix = None
        self.dist_coeffs = None
        
        # Feature detection
        self.feature_detector = cv2.ORB_create(nfeatures=self.max_features)
        self.descriptor_extractor = cv2.ORB_create()
        self.matcher = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=False)
        
        # State
        self.prev_image = None
        self.prev_keypoints = None
        self.prev_descriptors = None
        self.prev_time = None
        self.current_pose = np.eye(3)  # 2D pose [x, y, theta]
        
        # CV Bridge
        self.cv_bridge = CvBridge()
        
        # Publishers
        self.odom_pub = self.create_publisher(
            Odometry, '/visual_odometry', 10
        )
        
        # Subscribers
        camera_qos = QoSProfile(
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=5,
            durability=QoSDurabilityPolicy.VOLATILE,
        )
        
        self.image_sub = self.create_subscription(
            Image, '/camera/image_raw', self.image_callback, camera_qos
        )
        
        self.camera_info_sub = self.create_subscription(
            CameraInfo, '/camera/camera_info', self.camera_info_callback, 10
        )
        
        self.get_logger().info('Visual odometry node initialized')
        
    def camera_info_callback(self, msg: CameraInfo):
        """Handle camera calibration info."""
        if self.camera_matrix is None:
            self.camera_matrix = np.array(msg.k).reshape(3, 3)
            self.dist_coeffs = np.array(msg.d)
            self.get_logger().info('Camera calibration received')
    
    def image_callback(self, msg: Image):
        """Process camera images for visual odometry."""
        try:
            # Convert ROS image to OpenCV
            cv_image = self.cv_bridge.imgmsg_to_cv2(msg, desired_encoding='bgr8')
            gray = cv2.cvtColor(cv_image, cv2.COLOR_BGR2GRAY)
            
            current_time = self.get_clock().now()
            
            if self.prev_image is None:
                # Initialize
                self.prev_image = gray
                self.prev_time = current_time
                return
            
            # Calculate time delta
            dt = (current_time - self.prev_time).nanoseconds / 1e9
            if dt < 0.01:  # Minimum 10ms between frames
                return
            
            # Detect features
            keypoints = self.feature_detector.detect(gray, None)
            if len(keypoints) < self.min_matches:
                self.get_logger().warn(f'Insufficient features: {len(keypoints)}')
                self.prev_image = gray
                self.prev_time = current_time
                return
            
            # Extract descriptors
            keypoints, descriptors = self.descriptor_extractor.compute(gray, keypoints)
            
            if self.prev_descriptors is not None:
                # Match features
                matches = self.matcher.knnMatch(self.prev_descriptors, descriptors, k=2)
                
                # Apply Lowe's ratio test
                good_matches = []
                for match_pair in matches:
                    if len(match_pair) == 2:
                        m, n = match_pair
                        if m.distance < 0.75 * n.distance:
                            good_matches.append(m)
                
                if len(good_matches) >= self.min_matches:
                    # Estimate motion
                    self.estimate_motion(good_matches, self.prev_keypoints, keypoints, dt)
            
            # Update previous frame
            self.prev_image = gray
            self.prev_keypoints = keypoints
            self.prev_descriptors = descriptors
            self.prev_time = current_time
            
        except Exception as e:
            self.get_logger().error(f'Image processing error: {e}')
    
    def estimate_motion(self, matches, prev_kp, curr_kp, dt):
        """Estimate robot motion from feature matches."""
        # Extract matched points
        prev_pts = np.float32([prev_kp[m.queryIdx].pt for m in matches]).reshape(-1, 1, 2)
        curr_pts = np.float32([curr_kp[m.trainIdx].pt for m in matches]).reshape(-1, 1, 2)
        
        # Calculate optical flow
        flow = curr_pts - prev_pts
        
        # Filter outliers based on distance
        distances = np.linalg.norm(flow, axis=2)
        median_dist = np.median(distances)
        inlier_mask = distances < (median_dist * 2.0)
        
        if np.sum(inlier_mask) < self.min_matches:
            return
        
        # Use inliers for motion estimation
        prev_pts_inlier = prev_pts[inlier_mask]
        curr_pts_inlier = curr_pts[inlier_mask]
        
        # Estimate translation (simplified 2D)
        translation = np.mean(curr_pts_inlier - prev_pts_inlier, axis=0)
        
        # Convert to robot frame (assuming camera facing forward)
        dx = translation[0, 0] * self.scale_factor
        dy = -translation[0, 1] * self.scale_factor  # Camera y is robot -x
        
        # Estimate rotation from dominant motion direction
        angles = np.arctan2(flow[inlier_mask, 0, 1], flow[inlier_mask, 0, 0])
        dtheta = np.median(angles) * 0.1  # Scale down rotation estimate
        
        # Update pose (simplified dead reckoning)
        cos_theta = np.cos(self.current_pose[2])
        sin_theta = np.sin(self.current_pose[2])
        
        self.current_pose[0] += dx * cos_theta - dy * sin_theta
        self.current_pose[1] += dx * sin_theta + dy * cos_theta
        self.current_pose[2] += dtheta
        
        # Calculate velocities
        vx = dx / dt
        vy = dy / dt
        vtheta = dtheta / dt
        
        # Publish odometry
        self.publish_odometry(vx, vy, vtheta)
    
    def publish_odometry(self, vx, vy, vtheta):
        """Publish visual odometry message."""
        odom = Odometry()
        odom.header.stamp = self.get_clock().now().to_msg()
        odom.header.frame_id = 'odom'
        odom.child_frame_id = 'base_link'
        
        # Pose
        odom.pose.pose.position.x = self.current_pose[0]
        odom.pose.pose.position.y = self.current_pose[1]
        odom.pose.pose.position.z = 0.0
        
        # Orientation from yaw
        import tf_transformations
        quat = tf_transformations.quaternion_from_euler(0, 0, self.current_pose[2])
        odom.pose.pose.orientation.x = quat[0]
        odom.pose.pose.orientation.y = quat[1]
        odom.pose.pose.orientation.z = quat[2]
        odom.pose.pose.orientation.w = quat[3]
        
        # Twist
        odom.twist.twist.linear.x = vx
        odom.twist.twist.linear.y = vy
        odom.twist.twist.angular.z = vtheta
        
        # Covariance (simplified)
        odom.pose.covariance = [0.1] * 36
        odom.twist.covariance = [0.1] * 36
        
        self.odom_pub.publish(odom)


def main():
    rclpy.init()
    node = VisualOdometryNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()